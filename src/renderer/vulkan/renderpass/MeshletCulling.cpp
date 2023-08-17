////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MeshletCulling.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/ShaderModules.h"

#include "renderer/graph/FrameGraphBuilder.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"
#include "core/memory/Allocator.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/shader/meshlet/meshlet.share.hlsl"
#include "renderer/shader/meshlet/meshlet_culling.share.hlsl"

namespace Reaper
{
constexpr u32 IndexSizeBytes = 1;
// NOTE: Because of u8 indices we pack a triangle in 24 bits + 8 bits for a prim restart
constexpr u32 TriangleIndicesSizeBytes = 4;
constexpr u32 MaxMeshletCullingPassCount = 4;
constexpr u32 MaxMeshInstanceCount = 512;
// NOTE: Increasing this seems to make perf degrade noticeably on my intel iGPU for the same amount of geometry drawn.
constexpr u32 MaxVisibleMeshletsPerPass = 4096;

// Worst case if all meshlets of all passes aren't culled.
// This shouldn't happen, we can probably cut this by half and raise a warning when we cross the limit.
constexpr u64 VisibleIndexBufferSizeBytes =
    MaxVisibleMeshletsPerPass * MaxMeshletCullingPassCount * MeshletMaxTriangleCount * TriangleIndicesSizeBytes;
constexpr u32 MaxIndirectDrawCountPerPass = MaxVisibleMeshletsPerPass;

CullMeshletsFrameGraphData create_cull_meshlet_frame_graph_data(FrameGraph::Builder& builder)
{
    CullMeshletsFrameGraphData::Clear clear;

    clear.pass_handle = builder.create_render_pass("Meshlet Culling Clear");

    clear.meshlet_counters = builder.create_buffer(
        clear.pass_handle, "Meshlet counters",
        DefaultGPUBufferProperties(CountersCount * MaxMeshletCullingPassCount, sizeof(u32),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::TransferSrc
                                       | GPUBufferUsage::TransferDst | GPUBufferUsage::StorageBuffer),
        GPUBufferAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    CullMeshletsFrameGraphData::CullMeshlets cull_meshlets;

    cull_meshlets.pass_handle = builder.create_render_pass("Cull Meshlets");

    cull_meshlets.meshlet_counters =
        builder.write_buffer(cull_meshlets.pass_handle, clear.meshlet_counters,
                             GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT});

    CullMeshletsFrameGraphData::CullTriangles cull_triangles;

    cull_triangles.pass_handle = builder.create_render_pass("Cull Triangles");

    cull_triangles.meshlet_counters =
        builder.write_buffer(cull_triangles.pass_handle, cull_meshlets.meshlet_counters,
                             GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT});

    CullMeshletsFrameGraphData::Debug debug;

    debug.pass_handle = builder.create_render_pass("Debug", true);

    debug.meshlet_counters =
        builder.read_buffer(debug.pass_handle, cull_triangles.meshlet_counters,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT});

    return CullMeshletsFrameGraphData{
        .clear = clear,
        .cull_meshlets = cull_meshlets,
        .cull_triangles = cull_triangles,
        .debug = debug,
    };
}

MeshletCullingResources create_meshlet_culling_resources(ReaperRoot& root, VulkanBackend& backend,
                                                         const ShaderModules& shader_modules)
{
    MeshletCullingResources resources;

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout = create_descriptor_set_layout(backend.device, bindings);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline = create_compute_pipeline(backend.device, pipelineLayout, shader_modules.cull_meshlet_cs);

        resources.cull_meshlets_pipe = SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout = create_descriptor_set_layout(backend.device, bindings);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipelineLayout, shader_modules.prepare_fine_culling_indirect_cs);

        resources.cull_meshlets_prep_indirect_pipe = SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipelineLayout, shader_modules.cull_triangle_batch_cs);

        resources.cull_triangles_pipe = SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    resources.mesh_instance_buffer =
        create_buffer(root, backend.device, "Culling instance constants",
                      DefaultGPUBufferProperties(MaxMeshInstanceCount * MaxMeshletCullingPassCount,
                                                 sizeof(CullMeshInstanceParams), GPUBufferUsage::StorageBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.counters_cpu_buffer = create_buffer(root, backend.device, "Meshlet counters CPU",
                                                  DefaultGPUBufferProperties(CountersCount * MaxMeshletCullingPassCount,
                                                                             sizeof(u32), GPUBufferUsage::TransferDst),
                                                  backend.vma_instance, MemUsage::CPU_Only);

    resources.visible_meshlet_offsets_buffer =
        create_buffer(root, backend.device, "Visible meshlet offsets buffer",
                      DefaultGPUBufferProperties(MaxVisibleMeshletsPerPass * MaxMeshletCullingPassCount,
                                                 sizeof(MeshletOffsets), GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.triangle_culling_indirect_dispatch_buffer =
        create_buffer(root, backend.device, "Indirect dispatch buffer",
                      DefaultGPUBufferProperties(MaxMeshletCullingPassCount, sizeof(VkDispatchIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.visible_index_buffer =
        create_buffer(root, backend.device, "Meshlet visible index buffer",
                      DefaultGPUBufferProperties(VisibleIndexBufferSizeBytes * MaxMeshletCullingPassCount, 1,
                                                 GPUBufferUsage::IndexBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.visible_indirect_draw_commands_buffer =
        create_buffer(root, backend.device, "Meshlet Indirect draw commands buffer",
                      DefaultGPUBufferProperties(MaxIndirectDrawCountPerPass * MaxMeshletCullingPassCount,
                                                 sizeof(VkDrawIndexedIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    // NOTE: This buffer should only be used in the main pass.
    resources.visible_meshlet_buffer = create_buffer(
        root, backend.device, "Visible meshlet buffer",
        DefaultGPUBufferProperties(MaxIndirectDrawCountPerPass, sizeof(VisibleMeshlet), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    Assert(MaxIndirectDrawCountPerPass < backend.physicalDeviceProperties.limits.maxDrawIndirectCount);

    resources.cull_meshlet_descriptor_sets.resize(4);
    resources.cull_triangles_descriptor_sets.resize(4);

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, resources.cull_meshlets_pipe.descSetLayout,
                             resources.cull_meshlet_descriptor_sets);
    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             resources.cull_meshlets_prep_indirect_pipe.descSetLayout,
                             nonstd::span(&resources.cull_prepare_descriptor_set, 1));
    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             resources.cull_triangles_pipe.descSetLayout, resources.cull_triangles_descriptor_sets);

    const VkEventCreateInfo event_info = {
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
    };

    Assert(vkCreateEvent(backend.device, &event_info, nullptr, &resources.countersReadyEvent) == VK_SUCCESS);
    VulkanSetDebugName(backend.device, resources.countersReadyEvent, "Counters ready event");

    return resources;
}

namespace
{
    void destroy_simple_pipeline(VkDevice device, SimplePipeline simple_pipeline)
    {
        vkDestroyPipeline(device, simple_pipeline.pipeline, nullptr);
        vkDestroyPipelineLayout(device, simple_pipeline.pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, simple_pipeline.descSetLayout, nullptr);
    }
} // namespace

void destroy_meshlet_culling_resources(VulkanBackend& backend, MeshletCullingResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.counters_cpu_buffer.handle,
                     resources.counters_cpu_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.mesh_instance_buffer.handle,
                     resources.mesh_instance_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.visible_index_buffer.handle,
                     resources.visible_index_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.triangle_culling_indirect_dispatch_buffer.handle,
                     resources.triangle_culling_indirect_dispatch_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.visible_meshlet_offsets_buffer.handle,
                     resources.visible_meshlet_offsets_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.visible_indirect_draw_commands_buffer.handle,
                     resources.visible_indirect_draw_commands_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.visible_meshlet_buffer.handle,
                     resources.visible_meshlet_buffer.allocation);

    destroy_simple_pipeline(backend.device, resources.cull_meshlets_pipe);
    destroy_simple_pipeline(backend.device, resources.cull_meshlets_prep_indirect_pipe);
    destroy_simple_pipeline(backend.device, resources.cull_triangles_pipe);

    vkDestroyEvent(backend.device, resources.countersReadyEvent, nullptr);
}

void upload_meshlet_culling_resources(VulkanBackend& backend, const PreparedData& prepared,
                                      MeshletCullingResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.cull_mesh_instance_params.empty())
        return;

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.mesh_instance_buffer,
                                  prepared.cull_mesh_instance_params.data(),
                                  prepared.cull_mesh_instance_params.size() * sizeof(CullMeshInstanceParams));
}

void update_meshlet_culling_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                                 MeshletCullingResources& resources, const MeshCache& mesh_cache,
                                                 const FrameGraphBuffer& meshlet_counters)
{
    write_helper.append(resources.cull_prepare_descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        meshlet_counters.handle);
    write_helper.append(resources.cull_prepare_descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.triangle_culling_indirect_dispatch_buffer.handle);

    for (const CullPassData& cull_pass : prepared.cull_passes)
    {
        const u32 pass_index = cull_pass.pass_index;
        Assert(pass_index < MaxMeshletCullingPassCount);

        const GPUBufferView counter_buffer_view =
            get_buffer_view(meshlet_counters.properties, BufferSubresource{pass_index * CountersCount, CountersCount});
        const GPUBufferView visible_meshlet_offsets_view =
            get_buffer_view(resources.visible_meshlet_offsets_buffer.properties_deprecated,
                            BufferSubresource{pass_index * MaxVisibleMeshletsPerPass, MaxVisibleMeshletsPerPass});

        {
            VkDescriptorSet descriptor_set = resources.cull_meshlet_descriptor_sets[pass_index];
            write_helper.append(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_cache.meshletBuffer.handle);
            write_helper.append(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                resources.mesh_instance_buffer.handle);
            write_helper.append(descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshlet_counters.handle,
                                counter_buffer_view.offset_bytes, counter_buffer_view.size_bytes);
            write_helper.append(descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                resources.visible_meshlet_offsets_buffer.handle,
                                visible_meshlet_offsets_view.offset_bytes, visible_meshlet_offsets_view.size_bytes);
        }

        {
            const GPUBufferView visible_indices_view =
                get_buffer_view(resources.visible_index_buffer.properties_deprecated,
                                get_meshlet_visible_index_buffer_pass(pass_index));
            const GPUBufferView indirect_draw_view = get_buffer_view(
                resources.visible_indirect_draw_commands_buffer.properties_deprecated,
                BufferSubresource{pass_index * MaxIndirectDrawCountPerPass, MaxIndirectDrawCountPerPass});

            VkDescriptorSet descriptor_set = resources.cull_triangles_descriptor_sets[pass_index];
            write_helper.append(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                resources.visible_meshlet_offsets_buffer.handle,
                                visible_meshlet_offsets_view.offset_bytes, visible_meshlet_offsets_view.size_bytes);
            write_helper.append(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_cache.indexBuffer.handle);
            write_helper.append(descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                mesh_cache.vertexBufferPosition.handle);
            write_helper.append(descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                resources.mesh_instance_buffer.handle);
            write_helper.append(descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                resources.visible_index_buffer.handle, visible_indices_view.offset_bytes,
                                visible_indices_view.size_bytes);
            write_helper.append(descriptor_set, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                resources.visible_indirect_draw_commands_buffer.handle, indirect_draw_view.offset_bytes,
                                indirect_draw_view.size_bytes);
            write_helper.append(descriptor_set, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshlet_counters.handle,
                                counter_buffer_view.offset_bytes, counter_buffer_view.size_bytes);
            write_helper.append(descriptor_set, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                resources.visible_meshlet_buffer.handle);
        }
    }
}

void record_meshlet_culling_command_buffer(ReaperRoot& root, CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                           MeshletCullingResources& resources)
{
    u64              total_meshlet_count = 0;
    std::vector<u64> meshlet_count_per_pass;

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Cull Meshes");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cull_meshlets_pipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            u64& pass_meshlet_count = meshlet_count_per_pass.emplace_back();
            pass_meshlet_count = 0;

            vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    resources.cull_meshlets_pipe.pipelineLayout, 0, 1,
                                    &resources.cull_meshlet_descriptor_sets[cull_pass.pass_index], 0, nullptr);

            for (const CullCmd& command : cull_pass.cull_commands)
            {
                vkCmdPushConstants(cmdBuffer.handle, resources.cull_meshlets_pipe.pipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(command.push_constants),
                                   &command.push_constants);

                const u32 group_count_x = div_round_up(command.push_constants.meshlet_count, MeshletCullThreadCount);
                vkCmdDispatch(cmdBuffer.handle, group_count_x, command.instance_count, 1);

                pass_meshlet_count += command.push_constants.meshlet_count;
            }

            total_meshlet_count += pass_meshlet_count;
        }
    }

    log_debug(root, "CPU mesh stats:");
    for (auto meshlet_count : meshlet_count_per_pass)
    {
        log_debug(root, "- pass total submitted meshlets = {}, approx. triangles = {}", meshlet_count,
                  meshlet_count * MeshletMaxTriangleCount);
    }
    log_debug(root, "- total submitted meshlets = {}, approx. triangles = {}", total_meshlet_count,
              total_meshlet_count * MeshletMaxTriangleCount);

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", Color::Red);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }
}

void record_triangle_culling_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                            MeshletCullingResources& resources)
{
    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Indirect Prepare");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          resources.cull_meshlets_prep_indirect_pipe.pipeline);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                resources.cull_meshlets_prep_indirect_pipe.pipelineLayout, 0, 1,
                                &resources.cull_prepare_descriptor_set, 0, nullptr);

        const u32 group_count_x = div_round_up(prepared.cull_passes.size(), PrepareIndirectDispatchThreadCount);
        vkCmdDispatch(cmdBuffer.handle, group_count_x, 1, 1);
    }

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", Color::Red);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Cull Triangles");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cull_triangles_pipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    resources.cull_triangles_pipe.pipelineLayout, 0, 1,
                                    &resources.cull_triangles_descriptor_sets[cull_pass.pass_index], 0, nullptr);

            CullPushConstants consts;
            consts.output_size_ts = cull_pass.output_size_ts;
            consts.main_pass = cull_pass.main_pass ? 1 : 0;

            vkCmdPushConstants(cmdBuffer.handle, resources.cull_triangles_pipe.pipelineLayout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(consts), &consts);

            vkCmdDispatchIndirect(cmdBuffer.handle, resources.triangle_culling_indirect_dispatch_buffer.handle,
                                  cull_pass.pass_index * sizeof(VkDispatchIndirectCommand));
        }
    }

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", Color::Red);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                      VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }
}

void record_meshlet_culling_debug_command_buffer(CommandBuffer&           cmdBuffer,
                                                 MeshletCullingResources& resources,
                                                 const FrameGraphBuffer&  meshlet_counters)
{
    const VkBufferCopy2 region = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .pNext = nullptr,
        .srcOffset = 0,
        .dstOffset = 0,
        .size = meshlet_counters.properties.element_count * meshlet_counters.properties.element_size_bytes,
    };

    const VkCopyBufferInfo2 copy = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .pNext = nullptr,
        .srcBuffer = meshlet_counters.handle,
        .dstBuffer = resources.counters_cpu_buffer.handle,
        .regionCount = 1,
        .pRegions = &region,
    };

    vkCmdCopyBuffer2(cmdBuffer.handle, &copy);

    vkCmdResetEvent2(cmdBuffer.handle, resources.countersReadyEvent, VK_PIPELINE_STAGE_2_TRANSFER_BIT);

    const GPUBufferAccess src = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    const GPUBufferAccess dst = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
    const GPUBufferView   view = default_buffer_view(resources.counters_cpu_buffer.properties_deprecated);

    VkBufferMemoryBarrier2 bufferBarrier = get_vk_buffer_barrier(resources.counters_cpu_buffer.handle, view, src, dst);

    const VkDependencyInfo dependencies = VkDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = VK_FLAGS_NONE,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &bufferBarrier,
        .imageMemoryBarrierCount = 0,
        .pImageMemoryBarriers = nullptr,
    };

    vkCmdSetEvent2(cmdBuffer.handle, resources.countersReadyEvent, &dependencies);
}

std::vector<MeshletCullingStats> get_meshlet_culling_gpu_stats(VulkanBackend& backend, const PreparedData& prepared,
                                                               MeshletCullingResources& resources)
{
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(backend.vma_instance, resources.counters_cpu_buffer.allocation, &allocation_info);

    void* mapped_data_ptr = nullptr;
    u64   mappped_data_size_bytes =
        alignOffset(allocation_info.size, backend.physicalDeviceProperties.limits.nonCoherentAtomSize);

    Assert(vkMapMemory(backend.device, allocation_info.deviceMemory, allocation_info.offset, mappped_data_size_bytes, 0,
                       &mapped_data_ptr)
           == VK_SUCCESS);

    const VkMappedMemoryRange staging_range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .pNext = nullptr,
        .memory = allocation_info.deviceMemory,
        .offset = allocation_info.offset,
        .size = mappped_data_size_bytes,
    };

    vkInvalidateMappedMemoryRanges(backend.device, 1, &staging_range);

    Assert(mapped_data_ptr);

    std::vector<MeshletCullingStats> stats;

    for (u32 i = 0; i < prepared.cull_passes.size(); i++)
    {
        MeshletCullingStats& s = stats.emplace_back();
        s.pass_index = i;
        s.surviving_meshlet_count = static_cast<u32*>(mapped_data_ptr)[i * CountersCount + MeshletCounterOffset];
        s.surviving_triangle_count = static_cast<u32*>(mapped_data_ptr)[i * CountersCount + TriangleCounterOffset];
        s.indirect_draw_command_count =
            static_cast<u32*>(mapped_data_ptr)[i * CountersCount + DrawCommandCounterOffset];
    }

    vkUnmapMemory(backend.device, allocation_info.deviceMemory);

    return stats;
}

namespace
{
    VkIndexType get_vk_meshlet_index_type()
    {
        if (IndexSizeBytes == 1)
            return VK_INDEX_TYPE_UINT8_EXT;
        else if (IndexSizeBytes == 2)
            return VK_INDEX_TYPE_UINT16;
        else if (IndexSizeBytes == 4)
            return VK_INDEX_TYPE_UINT32;
        else
        {
            AssertUnreachable();
        }

        return VK_INDEX_TYPE_NONE_KHR;
    }
} // namespace

MeshletDrawParams get_meshlet_draw_params(u32 pass_index)
{
    MeshletDrawParams params;
    params.counter_buffer_offset = (pass_index * CountersCount + DrawCommandCounterOffset) * sizeof(u32);
    params.index_buffer_offset = pass_index * VisibleIndexBufferSizeBytes;
    params.index_type = get_vk_meshlet_index_type();
    params.command_buffer_offset = pass_index * MaxIndirectDrawCountPerPass * sizeof(VkDrawIndexedIndirectCommand);
    params.command_buffer_max_count = MaxIndirectDrawCountPerPass;

    return params;
}

BufferSubresource get_meshlet_visible_index_buffer_pass(u32 pass_index)
{
    return BufferSubresource{pass_index * VisibleIndexBufferSizeBytes, VisibleIndexBufferSizeBytes};
}
} // namespace Reaper
