////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Culling.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/Shader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <array>

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/shader/share/meshlet.hlsl"
#include "renderer/shader/share/meshlet_culling.hlsl"

namespace Reaper
{
constexpr u32 IndexSizeBytes = 4;
constexpr u32 MaxCullPassCount = 4;
constexpr u32 MaxCullInstanceCount = 512 * MaxCullPassCount;
constexpr u32 MaxSurvivingMeshletsPerPass = 200;

// Worst case if all meshlets of all passes aren't culled.
// This shouldn't happen, we can probably cut this by half and raise a warning when we cross the limit.
constexpr u64 DynamicIndexBufferSizeBytes =
    MaxSurvivingMeshletsPerPass * MaxCullPassCount * MeshletMaxTriangleCount * 3 * IndexSizeBytes;
constexpr u32 MaxIndirectDrawCountPerPass = MaxSurvivingMeshletsPerPass;

namespace
{
    std::vector<VkDescriptorSet> create_descriptor_sets(VulkanBackend& backend, VkDescriptorSetLayout set_layout,
                                                        u32 count)
    {
        std::vector<VkDescriptorSetLayout> layouts(count, set_layout);
        std::vector<VkDescriptorSet>       sets(layouts.size());

        const VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                       backend.global_descriptor_pool, static_cast<u32>(layouts.size()),
                                                       layouts.data()};

        Assert(vkAllocateDescriptorSets(backend.device, &allocInfo, sets.data()) == VK_SUCCESS);

        return sets;
    }

    void update_cull_meshlet_descriptor_sets(VulkanBackend& backend, CullResources& resources,
                                             const BufferInfo& meshletBuffer, u32 pass_index)
    {
        VkDescriptorSet descriptor_set = resources.cull_meshlet_descriptor_sets[pass_index];

        const VkDescriptorBufferInfo meshlets = default_descriptor_buffer_info(meshletBuffer);
        const VkDescriptorBufferInfo instanceParams =
            default_descriptor_buffer_info(resources.cullInstanceParamsBuffer);
        const VkDescriptorBufferInfo counters = get_vk_descriptor_buffer_info(
            resources.countersBuffer, BufferSubresource{pass_index * CountersCount, CountersCount});
        const VkDescriptorBufferInfo meshlet_offsets = get_vk_descriptor_buffer_info(
            resources.dynamicMeshletBuffer,
            BufferSubresource{pass_index * MaxSurvivingMeshletsPerPass, MaxSurvivingMeshletsPerPass});

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshlets),
            create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &instanceParams),
            create_buffer_descriptor_write(descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &counters),
            create_buffer_descriptor_write(descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshlet_offsets),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    void update_culling_descriptor_sets(VulkanBackend& backend, CullResources& resources,
                                        const BufferInfo& staticIndexBuffer, const BufferInfo& vertexBufferPosition,
                                        u32 pass_index)
    {
        VkDescriptorSet descriptor_set = resources.cull_triangles_descriptor_sets[pass_index];

        const VkDescriptorBufferInfo meshlets = get_vk_descriptor_buffer_info(
            resources.dynamicMeshletBuffer,
            BufferSubresource{pass_index * MaxSurvivingMeshletsPerPass, MaxSurvivingMeshletsPerPass});
        const VkDescriptorBufferInfo indices = default_descriptor_buffer_info(staticIndexBuffer);
        const VkDescriptorBufferInfo vertexPositions = default_descriptor_buffer_info(vertexBufferPosition);
        const VkDescriptorBufferInfo instanceParams =
            default_descriptor_buffer_info(resources.cullInstanceParamsBuffer);
        const VkDescriptorBufferInfo indicesOut = get_vk_descriptor_buffer_info(
            resources.dynamicIndexBuffer,
            BufferSubresource{pass_index * DynamicIndexBufferSizeBytes, DynamicIndexBufferSizeBytes});
        const VkDescriptorBufferInfo drawCommandOut = get_vk_descriptor_buffer_info(
            resources.indirectDrawBuffer,
            BufferSubresource{pass_index * MaxIndirectDrawCountPerPass, MaxIndirectDrawCountPerPass});
        const VkDescriptorBufferInfo countersOut = get_vk_descriptor_buffer_info(
            resources.countersBuffer, BufferSubresource{pass_index * CountersCount, CountersCount});

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshlets),
            create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indices),
            create_buffer_descriptor_write(descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &vertexPositions),
            create_buffer_descriptor_write(descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &instanceParams),
            create_buffer_descriptor_write(descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indicesOut),
            create_buffer_descriptor_write(descriptor_set, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &drawCommandOut),
            create_buffer_descriptor_write(descriptor_set, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &countersOut),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    void update_cull_prepare_indirect_descriptor_set(VulkanBackend& backend, CullResources& resources,
                                                     VkDescriptorSet descriptor_set)
    {
        const VkDescriptorBufferInfo counters = default_descriptor_buffer_info(resources.countersBuffer);
        const VkDescriptorBufferInfo indirectCommands =
            default_descriptor_buffer_info(resources.indirectDispatchBuffer);

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &counters),
            create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indirectCommands),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    SimplePipeline create_meshlet_prepare_indirect_pipeline(VulkanBackend& backend)
    {
        VkShaderModule computeShader =
            vulkan_create_shader_module(backend.device, "build/shader/prepare_fine_culling_indirect.comp.spv");

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline = create_compute_pipeline(backend.device, pipelineLayout, computeShader);

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        return SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    SimplePipeline create_cull_meshlet_pipeline(VulkanBackend& backend)
    {
        VkShaderModule computeShader =
            vulkan_create_shader_module(backend.device, "build/shader/cull_meshlet.comp.spv");

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline = create_compute_pipeline(backend.device, pipelineLayout, computeShader);

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        return SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    SimplePipeline create_cull_triangles_pipeline(VulkanBackend& backend)
    {
        VkShaderModule computeShader =
            vulkan_create_shader_module(backend.device, "build/shader/cull_triangle_batch.comp.spv");

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline = create_compute_pipeline(backend.device, pipelineLayout, computeShader);

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        return SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }
} // namespace

CullResources create_culling_resources(ReaperRoot& root, VulkanBackend& backend)
{
    CullResources resources;

    resources.cullMeshletPipe = create_cull_meshlet_pipeline(backend);
    resources.cullMeshletPrepIndirect = create_meshlet_prepare_indirect_pipeline(backend);
    resources.cullTrianglesPipe = create_cull_triangles_pipeline(backend);

    resources.cullInstanceParamsBuffer = create_buffer(
        root, backend.device, "Culling instance constants",
        DefaultGPUBufferProperties(MaxCullInstanceCount, sizeof(CullMeshInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.countersBuffer =
        create_buffer(root, backend.device, "Meshlet counters",
                      DefaultGPUBufferProperties(CountersCount * MaxCullPassCount, sizeof(u32),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::TransferSrc
                                                     | GPUBufferUsage::TransferDst | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.countersBufferCPU = create_buffer(
        root, backend.device, "Meshlet counters CPU",
        DefaultGPUBufferProperties(CountersCount * MaxCullPassCount, sizeof(u32), GPUBufferUsage::TransferDst),
        backend.vma_instance, MemUsage::CPU_Only);

    resources.dynamicMeshletBuffer =
        create_buffer(root, backend.device, "Dynamic meshlet offsets",
                      DefaultGPUBufferProperties(MaxSurvivingMeshletsPerPass * MaxCullPassCount, sizeof(MeshletOffsets),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.indirectDispatchBuffer =
        create_buffer(root, backend.device, "Indirect dispatch buffer",
                      DefaultGPUBufferProperties(MaxCullPassCount, sizeof(VkDispatchIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.dynamicIndexBuffer =
        create_buffer(root, backend.device, "Dynamic indices",
                      DefaultGPUBufferProperties(DynamicIndexBufferSizeBytes * MaxCullPassCount, 1,
                                                 GPUBufferUsage::IndexBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.indirectDrawBuffer = create_buffer(
        root, backend.device, "Indirect draw buffer",
        DefaultGPUBufferProperties(MaxIndirectDrawCountPerPass * MaxCullPassCount, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    Assert(MaxIndirectDrawCountPerPass < backend.physicalDeviceProperties.limits.maxDrawIndirectCount);

    // FIXME
    resources.cull_meshlet_descriptor_sets =
        create_descriptor_sets(backend, resources.cullMeshletPipe.descSetLayout, 4);
    resources.cull_prepare_descriptor_set =
        create_descriptor_sets(backend, resources.cullMeshletPrepIndirect.descSetLayout, 1).front();
    resources.cull_triangles_descriptor_sets =
        create_descriptor_sets(backend, resources.cullTrianglesPipe.descSetLayout, 4);

    const VkEventCreateInfo event_info = {
        VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        nullptr,
        VK_FLAGS_NONE,
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

void destroy_culling_resources(VulkanBackend& backend, CullResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.countersBuffer.handle, resources.countersBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.countersBufferCPU.handle, resources.countersBufferCPU.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.cullInstanceParamsBuffer.handle,
                     resources.cullInstanceParamsBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.dynamicIndexBuffer.handle,
                     resources.dynamicIndexBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.indirectDispatchBuffer.handle,
                     resources.indirectDispatchBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.dynamicMeshletBuffer.handle,
                     resources.dynamicMeshletBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.indirectDrawBuffer.handle,
                     resources.indirectDrawBuffer.allocation);

    destroy_simple_pipeline(backend.device, resources.cullMeshletPipe);
    destroy_simple_pipeline(backend.device, resources.cullMeshletPrepIndirect);
    destroy_simple_pipeline(backend.device, resources.cullTrianglesPipe);

    vkDestroyEvent(backend.device, resources.countersReadyEvent, nullptr);
}

void upload_culling_resources(VulkanBackend& backend, const PreparedData& prepared, CullResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.cull_mesh_instance_params.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, resources.cullInstanceParamsBuffer,
                       prepared.cull_mesh_instance_params.data(),
                       prepared.cull_mesh_instance_params.size() * sizeof(CullMeshInstanceParams));
}

void update_culling_pass_descriptor_sets(VulkanBackend& backend, const PreparedData& prepared, CullResources& resources,
                                         const MeshCache& mesh_cache)
{
    for (const CullPassData& cull_pass : prepared.cull_passes)
    {
        Assert(cull_pass.pass_index < MaxCullPassCount);
        update_cull_meshlet_descriptor_sets(backend, resources, mesh_cache.meshletBuffer, cull_pass.pass_index);
        update_culling_descriptor_sets(backend, resources, mesh_cache.indexBuffer, mesh_cache.vertexBufferPosition,
                                       cull_pass.pass_index);
    }

    update_cull_prepare_indirect_descriptor_set(backend, resources, resources.cull_prepare_descriptor_set);
}

void record_culling_command_buffer(ReaperRoot& root, CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                   CullResources& resources)
{
    u64              total_meshlet_count = 0;
    std::vector<u64> meshlet_count_per_pass;

    const u32 clear_value = 0;
    vkCmdFillBuffer(cmdBuffer.handle, resources.countersBuffer.handle, 0, VK_WHOLE_SIZE, clear_value);

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Cull Meshes");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cullMeshletPipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            u64& pass_meshlet_count = meshlet_count_per_pass.emplace_back();
            pass_meshlet_count = 0;

            vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    resources.cullMeshletPipe.pipelineLayout, 0, 1,
                                    &resources.cull_meshlet_descriptor_sets[cull_pass.pass_index], 0, nullptr);

            for (const CullCmd& command : cull_pass.cull_commands)
            {
                vkCmdPushConstants(cmdBuffer.handle, resources.cullMeshletPipe.pipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(command.push_constants),
                                   &command.push_constants);

                const u32 group_count_x = div_round_up(command.push_constants.meshlet_count, MeshletCullThreadCount);
                vkCmdDispatch(cmdBuffer.handle, group_count_x, command.instance_count, 1);

                pass_meshlet_count += command.push_constants.meshlet_count;
            }

            total_meshlet_count += pass_meshlet_count;
        }
    }

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Indirect Prepare");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cullMeshletPrepIndirect.pipeline);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                resources.cullMeshletPrepIndirect.pipelineLayout, 0, 1,
                                &resources.cull_prepare_descriptor_set, 0, nullptr);

        const u32 group_count_x = div_round_up(prepared.cull_passes.size(), PrepareIndirectDispatchThreadCount);
        vkCmdDispatch(cmdBuffer.handle, group_count_x, 1, 1);
    }

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Cull Triangles");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cullTrianglesPipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    resources.cullTrianglesPipe.pipelineLayout, 0, 1,
                                    &resources.cull_triangles_descriptor_sets[cull_pass.pass_index], 0, nullptr);

            CullPushConstants consts;
            consts.output_size_ts = cull_pass.output_size_ts;

            vkCmdPushConstants(cmdBuffer.handle, resources.cullTrianglesPipe.pipelineLayout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(consts), &consts);

            vkCmdDispatchIndirect(cmdBuffer.handle, resources.indirectDispatchBuffer.handle,
                                  cull_pass.pass_index * sizeof(VkDispatchIndirectCommand));
        }
    }

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                     VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        REAPER_GPU_SCOPE(cmdBuffer, "Copy counters");

        VkBufferCopy2 region = {};
        region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        region.pNext = nullptr;
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size =
            resources.countersBuffer.properties.element_count * resources.countersBuffer.properties.element_size_bytes;

        const VkCopyBufferInfo2 copy = {VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2, nullptr, resources.countersBuffer.handle,
                                        resources.countersBufferCPU.handle,   1,       &region};

        vkCmdCopyBuffer2(cmdBuffer.handle, &copy);

        vkCmdResetEvent2(cmdBuffer.handle, resources.countersReadyEvent, VK_PIPELINE_STAGE_2_TRANSFER_BIT);

        const GPUResourceAccess src = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        const GPUResourceAccess dst = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
        const GPUBufferView     view = default_buffer_view(resources.countersBufferCPU.properties);

        VkBufferMemoryBarrier2 bufferBarrier =
            get_vk_buffer_barrier(resources.countersBufferCPU.handle, view, src, dst);

        const VkDependencyInfo dependencies = VkDependencyInfo{
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 1, &bufferBarrier, 0, nullptr};

        vkCmdSetEvent2(cmdBuffer.handle, resources.countersReadyEvent, &dependencies);
    }

    log_debug(root, "CPU mesh stats:");
    for (auto meshlet_count : meshlet_count_per_pass)
    {
        log_debug(root, "- pass total submitted meshlets = {}, approx. triangles = {}", meshlet_count,
                  meshlet_count * MeshletMaxTriangleCount);
    }
    log_debug(root, "- total submitted meshlets = {}, approx. triangles = {}", total_meshlet_count,
              total_meshlet_count * MeshletMaxTriangleCount);
}

std::vector<CullingStats> get_gpu_culling_stats(VulkanBackend& backend, const PreparedData& prepared,
                                                CullResources& resources)
{
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(backend.vma_instance, resources.countersBufferCPU.allocation, &allocation_info);

    void* mapped_data_ptr = nullptr;

    Assert(vkMapMemory(backend.device, allocation_info.deviceMemory, allocation_info.offset, allocation_info.size, 0,
                       &mapped_data_ptr)
           == VK_SUCCESS);

    VkMappedMemoryRange staging_range = {};
    staging_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    staging_range.memory = allocation_info.deviceMemory;
    staging_range.offset = allocation_info.offset;
    staging_range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(backend.device, 1, &staging_range);

    Assert(mapped_data_ptr);

    std::vector<CullingStats> stats;

    for (u32 i = 0; i < prepared.cull_passes.size(); i++)
    {
        CullingStats& s = stats.emplace_back();
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
    constexpr VkIndexType get_vk_culling_index_type()
    {
        if constexpr (IndexSizeBytes == 2)
            return VK_INDEX_TYPE_UINT16;
        else
        {
            static_assert(IndexSizeBytes == 4, "Invalid index size");
            return VK_INDEX_TYPE_UINT32;
        }
    }
} // namespace

CullingDrawParams get_culling_draw_params(u32 pass_index)
{
    CullingDrawParams params;
    params.counter_buffer_offset = (pass_index * CountersCount + DrawCommandCounterOffset) * sizeof(u32);
    params.index_buffer_offset = pass_index * DynamicIndexBufferSizeBytes;
    params.index_type = get_vk_culling_index_type();
    params.command_buffer_offset = pass_index * MaxIndirectDrawCountPerPass * sizeof(VkDrawIndexedIndirectCommand);
    params.command_buffer_max_count = MaxIndirectDrawCountPerPass;

    return params;
}

} // namespace Reaper
