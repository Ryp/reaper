////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MeshletCulling.h"

#include "FrameGraphPass.h"

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
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/StorageBufferAllocator.h"
#include "renderer/vulkan/api/AssertHelper.h"

#include "renderer/graph/FrameGraphBuilder.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"
#include "core/memory/Allocator.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/shader/meshlet/meshlet.share.hlsl"
#include "renderer/shader/meshlet/meshlet_culling.share.hlsl"

namespace Reaper
{
namespace CullMeshlets
{
    enum BindingIndex
    {
        meshlets,
        cull_mesh_instance_params,
        Counters,
        meshlets_offsets_out,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{.slot = 0,
                          .count = 1,
                          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 1, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 2, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 3, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
    };
} // namespace CullMeshlets

namespace CullTrianglesPrepare
{
    enum BindingIndex
    {
        Counters,
        IndirectDispatchOut,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{.slot = 0,
                          .count = 1,
                          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 1, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
    };
} // namespace CullTrianglesPrepare

namespace CullTriangles
{
    enum BindingIndex
    {
        meshlets,
        Indices,
        buffer_position_ms,
        cull_mesh_instance_params,
        visible_index_buffer,
        DrawCommandOut,
        Counters,
        VisibleMeshlets,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{.slot = 0,
                          .count = 1,
                          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 1, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 2, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 3, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 4, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 5, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 6, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 7, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
    };
} // namespace CullTriangles

constexpr u32 IndexSizeBytes = 1;
// NOTE: Because of u8 indices we pack a triangle in 24 bits + 8 bits for a prim restart
constexpr u32 TriangleIndicesSizeBytes = 4;
constexpr u32 MaxMeshletCullingPassCount = 4;
// NOTE: Increasing this seems to make perf degrade noticeably on my intel iGPU for the same amount of geometry drawn.
constexpr u32 MaxVisibleMeshletsPerPass = 4096;

// Worst case if all meshlets of all passes aren't culled.
// This shouldn't happen, we can probably cut this by half and raise a warning when we cross the limit.
constexpr u64 VisibleIndexBufferSizeBytes =
    MaxVisibleMeshletsPerPass * MaxMeshletCullingPassCount * MeshletMaxTriangleCount * TriangleIndicesSizeBytes;
constexpr u32 MaxIndirectDrawCountPerPass = MaxVisibleMeshletsPerPass;

namespace
{
    VkPipeline create_cull_meshlet_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                            VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.cull_meshlet_cs);
    }
    VkPipeline create_cull_triangle_prepare_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                                     VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.prepare_fine_culling_indirect_cs);
    }
    VkPipeline create_cull_triangle_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                             VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.cull_triangle_batch_cs);
    }
} // namespace
CullMeshletsFrameGraphRecord create_cull_meshlet_frame_graph_record(FrameGraph::Builder& builder)
{
    CullMeshletsFrameGraphRecord::Clear clear;

    clear.pass_handle = builder.create_render_pass("Meshlet Culling Clear");

    clear.meshlet_counters = builder.create_buffer(
        clear.pass_handle, "Meshlet counters",
        DefaultGPUBufferProperties(CountersCount * MaxMeshletCullingPassCount, sizeof(u32),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::TransferSrc
                                       | GPUBufferUsage::TransferDst | GPUBufferUsage::StorageBuffer),
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    CullMeshletsFrameGraphRecord::CullMeshlets cull_meshlets;

    cull_meshlets.pass_handle = builder.create_render_pass("Cull Meshlets");

    cull_meshlets.meshlet_counters = builder.write_buffer(
        cull_meshlets.pass_handle, clear.meshlet_counters,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT});

    cull_meshlets.visible_meshlet_offsets =
        builder.create_buffer(cull_meshlets.pass_handle,
                              "Visible meshlet offsets buffer",
                              DefaultGPUBufferProperties(MaxVisibleMeshletsPerPass * MaxMeshletCullingPassCount,
                                                         sizeof(MeshletOffsets), GPUBufferUsage::StorageBuffer),
                              GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    CullMeshletsFrameGraphRecord::CullTrianglesPrepare cull_triangles_prepare;

    cull_triangles_prepare.pass_handle = builder.create_render_pass("Cull Triangles Prepare");

    cull_triangles_prepare.meshlet_counters =
        builder.read_buffer(cull_triangles_prepare.pass_handle, cull_meshlets.meshlet_counters,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    cull_triangles_prepare.indirect_dispatch_buffer = builder.create_buffer(
        cull_triangles_prepare.pass_handle,
        "Meshlet indirect dispatch buffer",
        DefaultGPUBufferProperties(MaxMeshletCullingPassCount, sizeof(VkDispatchIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    CullMeshletsFrameGraphRecord::CullTriangles cull_triangles;

    cull_triangles.pass_handle = builder.create_render_pass("Cull Triangles");

    cull_triangles.indirect_dispatch_buffer = builder.read_buffer(
        cull_triangles.pass_handle, cull_triangles_prepare.indirect_dispatch_buffer,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    cull_triangles.meshlet_counters =
        builder.write_buffer(cull_triangles.pass_handle, cull_meshlets.meshlet_counters,
                             GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT});

    cull_triangles.visible_meshlet_offsets =
        builder.read_buffer(cull_triangles.pass_handle, cull_meshlets.visible_meshlet_offsets,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    cull_triangles.meshlet_indirect_draw_commands = builder.create_buffer(
        cull_triangles.pass_handle, "Meshlet Indirect draw commands buffer",
        DefaultGPUBufferProperties(MaxIndirectDrawCountPerPass * MaxMeshletCullingPassCount,
                                   sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    cull_triangles.meshlet_visible_index_buffer =
        builder.create_buffer(cull_triangles.pass_handle, "Meshlet visible index buffer",
                              DefaultGPUBufferProperties(VisibleIndexBufferSizeBytes * MaxMeshletCullingPassCount, 1,
                                                         GPUBufferUsage::IndexBuffer | GPUBufferUsage::StorageBuffer),
                              GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    // NOTE: This buffer should only be used in the main pass.
    cull_triangles.visible_meshlet_buffer = builder.create_buffer(
        cull_triangles.pass_handle, "Visible meshlet buffer",
        DefaultGPUBufferProperties(MaxIndirectDrawCountPerPass, sizeof(VisibleMeshlet), GPUBufferUsage::StorageBuffer),
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    CullMeshletsFrameGraphRecord::Debug debug;

    debug.pass_handle = builder.create_render_pass("Debug", true);

    debug.meshlet_counters =
        builder.read_buffer(debug.pass_handle, cull_triangles.meshlet_counters,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT});

    return CullMeshletsFrameGraphRecord{
        .clear = clear,
        .cull_meshlets = cull_meshlets,
        .cull_triangles_prepare = cull_triangles_prepare,
        .cull_triangles = cull_triangles,
        .debug = debug,
    };
}

MeshletCullingResources create_meshlet_culling_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory)
{
    MeshletCullingResources resources;

    {
        using namespace CullMeshlets;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        VkDescriptorSetLayout descriptorSetLayout = create_descriptor_set_layout(backend.device, layout_bindings);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptorSetLayout, 1),
                                                                 std::span(&cullPushConstantRange, 1));

        u32 pipeline_index = register_pipeline_creator(pipeline_factory,
                                                       PipelineCreator{
                                                           .pipeline_layout = pipelineLayout,
                                                           .pipeline_creation_function = &create_cull_meshlet_pipeline,
                                                       });

        resources.cull_meshlets_pipe = SimplePipeline{pipeline_index, pipelineLayout, descriptorSetLayout};
    }

    {
        using namespace CullTrianglesPrepare;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        VkDescriptorSetLayout descriptorSetLayout = create_descriptor_set_layout(backend.device, layout_bindings);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptorSetLayout, 1),
                                                                 std::span(&cullPushConstantRange, 1));

        u32 pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipelineLayout,
                                          .pipeline_creation_function = &create_cull_triangle_prepare_pipeline,
                                      });

        resources.cull_meshlets_prep_indirect_pipe =
            SimplePipeline{pipeline_index, pipelineLayout, descriptorSetLayout};
    }

    {
        using namespace CullTriangles;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        VkDescriptorSetLayout descriptorSetLayout = create_descriptor_set_layout(backend.device, layout_bindings);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptorSetLayout, 1),
                                                                 std::span(&cullPushConstantRange, 1));

        u32 pipeline_index = register_pipeline_creator(pipeline_factory,
                                                       PipelineCreator{
                                                           .pipeline_layout = pipelineLayout,
                                                           .pipeline_creation_function = &create_cull_triangle_pipeline,
                                                       });

        resources.cull_triangles_pipe = SimplePipeline{pipeline_index, pipelineLayout, descriptorSetLayout};
    }

    resources.counters_cpu_properties = DefaultGPUBufferProperties(CountersCount * MaxMeshletCullingPassCount,
                                                                   sizeof(u32), GPUBufferUsage::TransferDst);

    resources.counters_cpu_buffer =
        create_buffer(backend.device, "Meshlet counters CPU", resources.counters_cpu_properties, backend.vma_instance,
                      MemUsage::CPU_Only);

    Assert(MaxIndirectDrawCountPerPass < backend.physical_device.properties.limits.maxDrawIndirectCount);

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, resources.cull_meshlets_pipe.descSetLayout,
                             resources.cull_meshlet_descriptor_sets);
    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.cull_meshlets_prep_indirect_pipe.descSetLayout, 1),
                             std::span(&resources.cull_prepare_descriptor_set, 1));
    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             resources.cull_triangles_pipe.descSetLayout, resources.cull_triangles_descriptor_sets);

    const VkEventCreateInfo event_info = {
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
    };

    AssertVk(vkCreateEvent(backend.device, &event_info, nullptr, &resources.countersReadyEvent));
    VulkanSetDebugName(backend.device, resources.countersReadyEvent, "Counters ready event");

    return resources;
}

namespace
{
    void destroy_simple_pipeline(VkDevice device, SimplePipeline simple_pipeline)
    {
        vkDestroyPipelineLayout(device, simple_pipeline.pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, simple_pipeline.descSetLayout, nullptr);
    }
} // namespace

void destroy_meshlet_culling_resources(VulkanBackend& backend, MeshletCullingResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.counters_cpu_buffer.handle,
                     resources.counters_cpu_buffer.allocation);

    destroy_simple_pipeline(backend.device, resources.cull_meshlets_pipe);
    destroy_simple_pipeline(backend.device, resources.cull_meshlets_prep_indirect_pipe);
    destroy_simple_pipeline(backend.device, resources.cull_triangles_pipe);

    vkDestroyEvent(backend.device, resources.countersReadyEvent, nullptr);
}

namespace
{
    void update_meshlet_culling_descriptor_sets(const FrameGraph::FrameGraph&                     frame_graph,
                                                const FrameGraphResources&                        frame_graph_resources,
                                                const CullMeshletsFrameGraphRecord::CullMeshlets& cull_meshlet_record,
                                                DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                                MeshletCullingResources& resources, const MeshCache& mesh_cache,
                                                const StorageBufferAlloc& mesh_instance_alloc)
    {
        const FrameGraphBuffer meshlet_counters =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, cull_meshlet_record.meshlet_counters);
        const FrameGraphBuffer visible_meshlet_offsets =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, cull_meshlet_record.visible_meshlet_offsets);

        using namespace CullMeshlets;

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            const u32 pass_index = cull_pass.pass_index;
            Assert(pass_index < MaxMeshletCullingPassCount);

            const GPUBufferView counter_buffer_view = get_buffer_view(
                meshlet_counters.properties, BufferSubresource{pass_index * CountersCount, CountersCount});

            const GPUBufferView visible_meshlet_offsets_view =
                get_buffer_view(visible_meshlet_offsets.properties,
                                BufferSubresource{pass_index * MaxVisibleMeshletsPerPass, MaxVisibleMeshletsPerPass});

            VkDescriptorSet descriptor_set = resources.cull_meshlet_descriptor_sets[pass_index];

            write_helper.append(descriptor_set, g_bindings[meshlets], mesh_cache.meshletBuffer.handle);
            write_helper.append(descriptor_set, g_bindings[cull_mesh_instance_params], mesh_instance_alloc.buffer,
                                mesh_instance_alloc.offset_bytes, mesh_instance_alloc.size_bytes);
            write_helper.append(descriptor_set, g_bindings[Counters], meshlet_counters.handle,
                                counter_buffer_view.offset_bytes, counter_buffer_view.size_bytes);
            write_helper.append(descriptor_set, g_bindings[meshlets_offsets_out], visible_meshlet_offsets.handle,
                                visible_meshlet_offsets_view.offset_bytes, visible_meshlet_offsets_view.size_bytes);
        }
    }

    void update_triangle_culling_prepare_descriptor_sets(
        const FrameGraph::FrameGraph&                             frame_graph,
        const FrameGraphResources&                                frame_graph_resources,
        const CullMeshletsFrameGraphRecord::CullTrianglesPrepare& cull_triangles_prepare_record,
        DescriptorWriteHelper&                                    write_helper,
        MeshletCullingResources&                                  resources)
    {
        const FrameGraphBuffer meshlet_counters =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, cull_triangles_prepare_record.meshlet_counters);
        const FrameGraphBuffer indirect_dispatch_buffer = get_frame_graph_buffer(
            frame_graph_resources, frame_graph, cull_triangles_prepare_record.indirect_dispatch_buffer);

        using namespace CullTrianglesPrepare;

        write_helper.append(resources.cull_prepare_descriptor_set, g_bindings[Counters], meshlet_counters.handle);
        write_helper.append(resources.cull_prepare_descriptor_set, g_bindings[IndirectDispatchOut],
                            indirect_dispatch_buffer.handle);
    }

    void update_triangle_culling_descriptor_sets(
        const FrameGraph::FrameGraph& frame_graph, const FrameGraphResources& frame_graph_resources,
        const CullMeshletsFrameGraphRecord::CullTriangles& cull_triangles_record, DescriptorWriteHelper& write_helper,
        const PreparedData& prepared, MeshletCullingResources& resources, const MeshCache& mesh_cache,
        const StorageBufferAlloc& mesh_instance_alloc)
    {
        const FrameGraphBuffer meshlet_counters =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, cull_triangles_record.meshlet_counters);
        const FrameGraphBuffer visible_meshlet_offsets =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, cull_triangles_record.visible_meshlet_offsets);
        const FrameGraphBuffer meshlet_indirect_draw_commands = get_frame_graph_buffer(
            frame_graph_resources, frame_graph, cull_triangles_record.meshlet_indirect_draw_commands);
        const FrameGraphBuffer meshlet_visible_index_buffer = get_frame_graph_buffer(
            frame_graph_resources, frame_graph, cull_triangles_record.meshlet_visible_index_buffer);
        const FrameGraphBuffer visible_meshlet_buffer =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, cull_triangles_record.visible_meshlet_buffer);

        using namespace CullTriangles;

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            const u32 pass_index = cull_pass.pass_index;
            Assert(pass_index < MaxMeshletCullingPassCount);

            const GPUBufferView counter_buffer_view = get_buffer_view(
                meshlet_counters.properties, BufferSubresource{pass_index * CountersCount, CountersCount});

            const GPUBufferView visible_meshlet_offsets_view =
                get_buffer_view(visible_meshlet_offsets.properties,
                                BufferSubresource{pass_index * MaxVisibleMeshletsPerPass, MaxVisibleMeshletsPerPass});

            const GPUBufferView visible_indices_view = get_buffer_view(
                meshlet_visible_index_buffer.properties, get_meshlet_visible_index_buffer_pass(pass_index));

            const GPUBufferView indirect_draw_view = get_buffer_view(
                meshlet_indirect_draw_commands.properties,
                BufferSubresource{pass_index * MaxIndirectDrawCountPerPass, MaxIndirectDrawCountPerPass});

            VkDescriptorSet descriptor_set = resources.cull_triangles_descriptor_sets[pass_index];

            write_helper.append(descriptor_set, g_bindings[meshlets], visible_meshlet_offsets.handle,
                                visible_meshlet_offsets_view.offset_bytes, visible_meshlet_offsets_view.size_bytes);
            write_helper.append(descriptor_set, g_bindings[Indices], mesh_cache.indexBuffer.handle);
            write_helper.append(descriptor_set, g_bindings[buffer_position_ms], mesh_cache.vertexBufferPosition.handle);
            write_helper.append(descriptor_set, g_bindings[cull_mesh_instance_params], mesh_instance_alloc.buffer,
                                mesh_instance_alloc.offset_bytes, mesh_instance_alloc.size_bytes);
            write_helper.append(descriptor_set, g_bindings[visible_index_buffer], meshlet_visible_index_buffer.handle,
                                visible_indices_view.offset_bytes, visible_indices_view.size_bytes);
            write_helper.append(descriptor_set, g_bindings[DrawCommandOut], meshlet_indirect_draw_commands.handle,
                                indirect_draw_view.offset_bytes, indirect_draw_view.size_bytes);
            write_helper.append(descriptor_set, g_bindings[Counters], meshlet_counters.handle,
                                counter_buffer_view.offset_bytes, counter_buffer_view.size_bytes);
            write_helper.append(descriptor_set, g_bindings[VisibleMeshlets], visible_meshlet_buffer.handle);
        }
    }
} // namespace

void update_meshlet_culling_passes_resources(const FrameGraph::FrameGraph&       frame_graph,
                                             const FrameGraphResources&          frame_graph_resources,
                                             const CullMeshletsFrameGraphRecord& record,
                                             DescriptorWriteHelper&              write_helper,
                                             StorageBufferAllocator&             frame_storage_allocator,
                                             const PreparedData&                 prepared,
                                             MeshletCullingResources&            resources,
                                             const MeshCache&                    mesh_cache)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.cull_mesh_instance_params.empty())
        return;

    Assert(!prepared.cull_mesh_instance_params.empty());

    const StorageBufferAlloc mesh_instance_alloc = allocate_storage(
        frame_storage_allocator, prepared.cull_mesh_instance_params.size() * sizeof(CullMeshInstanceParams));

    upload_storage_buffer(frame_storage_allocator, mesh_instance_alloc, prepared.cull_mesh_instance_params.data());

    update_meshlet_culling_descriptor_sets(frame_graph, frame_graph_resources, record.cull_meshlets, write_helper,
                                           prepared, resources, mesh_cache, mesh_instance_alloc);

    update_triangle_culling_prepare_descriptor_sets(frame_graph, frame_graph_resources, record.cull_triangles_prepare,
                                                    write_helper, resources);

    update_triangle_culling_descriptor_sets(frame_graph, frame_graph_resources, record.cull_triangles, write_helper,
                                            prepared, resources, mesh_cache, mesh_instance_alloc);
}

void record_meshlet_culling_clear_command_buffer(const FrameGraphHelper&                    frame_graph_helper,
                                                 const CullMeshletsFrameGraphRecord::Clear& pass_record,
                                                 CommandBuffer&                             cmdBuffer)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Meshlet Culling Clear");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const FrameGraphBuffer meshlet_counters = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.meshlet_counters);

    const u32 clear_value = 0;
    vkCmdFillBuffer(cmdBuffer.handle, meshlet_counters.handle, 0, VK_WHOLE_SIZE, clear_value);
}

void record_meshlet_culling_command_buffer(ReaperRoot& root, const FrameGraphHelper& frame_graph_helper,
                                           const CullMeshletsFrameGraphRecord::CullMeshlets& pass_record,
                                           CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                           const PreparedData& prepared, MeshletCullingResources& resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Cull Meshlets");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    u64              total_meshlet_count = 0;
    std::vector<u64> meshlet_count_per_pass;

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, resources.cull_meshlets_pipe.pipeline_index));

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
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(command.push_constants), &command.push_constants);

            const u32 group_count_x = div_round_up(command.push_constants.meshlet_count, MeshletCullThreadCount);
            vkCmdDispatch(cmdBuffer.handle, group_count_x, command.instance_count, 1);

            pass_meshlet_count += command.push_constants.meshlet_count;
        }

        total_meshlet_count += pass_meshlet_count;
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

void record_triangle_culling_prepare_command_buffer(
    const FrameGraphHelper& frame_graph_helper, const CullMeshletsFrameGraphRecord::CullTrianglesPrepare& pass_record,
    CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory, const PreparedData& prepared,
    MeshletCullingResources& resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Cull Meshlet Triangles Prepare");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, resources.cull_meshlets_prep_indirect_pipe.pipeline_index));

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            resources.cull_meshlets_prep_indirect_pipe.pipelineLayout, 0, 1,
                            &resources.cull_prepare_descriptor_set, 0, nullptr);

    const u32 group_count_x =
        div_round_up(static_cast<u32>(prepared.cull_passes.size()), PrepareIndirectDispatchThreadCount);
    vkCmdDispatch(cmdBuffer.handle, group_count_x, 1, 1);
}

void record_triangle_culling_command_buffer(const FrameGraphHelper&                            frame_graph_helper,
                                            const CullMeshletsFrameGraphRecord::CullTriangles& pass_record,
                                            CommandBuffer&                                     cmdBuffer,
                                            const PipelineFactory&                             pipeline_factory,
                                            const PreparedData&                                prepared,
                                            MeshletCullingResources&                           resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Cull Meshlet Triangles");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const FrameGraphBuffer indirect_dispatch_buffer = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.indirect_dispatch_buffer);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, resources.cull_triangles_pipe.pipeline_index));

    for (const CullPassData& cull_pass : prepared.cull_passes)
    {
        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                resources.cull_triangles_pipe.pipelineLayout, 0, 1,
                                &resources.cull_triangles_descriptor_sets[cull_pass.pass_index], 0, nullptr);

        CullPushConstants consts;
        consts.output_size_ts = cull_pass.output_size_ts;
        consts.main_pass = cull_pass.main_pass ? 1 : 0;

        vkCmdPushConstants(cmdBuffer.handle, resources.cull_triangles_pipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(consts), &consts);

        vkCmdDispatchIndirect(cmdBuffer.handle, indirect_dispatch_buffer.handle,
                              cull_pass.pass_index * sizeof(VkDispatchIndirectCommand));
    }
}

void record_meshlet_culling_debug_command_buffer(const FrameGraphHelper&                    frame_graph_helper,
                                                 const CullMeshletsFrameGraphRecord::Debug& pass_record,
                                                 CommandBuffer&                             cmdBuffer,
                                                 MeshletCullingResources&                   resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Meshlet Debug");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const FrameGraphBuffer meshlet_counters = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.meshlet_counters);

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

    const GPUBufferAccess src = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    const GPUBufferAccess dst = {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_NONE};
    const GPUBufferView   view = default_buffer_view(resources.counters_cpu_properties);

    VkBufferMemoryBarrier2 buffer_barrier = get_vk_buffer_barrier(resources.counters_cpu_buffer.handle, view, src, dst);

    const VkDependencyInfo dependencies = get_vk_buffer_barrier_depency_info(std::span(&buffer_barrier, 1));

    vkCmdSetEvent2(cmdBuffer.handle, resources.countersReadyEvent, &dependencies);

    vkCmdResetEvent2(cmdBuffer.handle, resources.countersReadyEvent, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
}

std::vector<MeshletCullingStats> get_meshlet_culling_gpu_stats(VulkanBackend& backend, const PreparedData& prepared,
                                                               MeshletCullingResources& resources)
{
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(backend.vma_instance, resources.counters_cpu_buffer.allocation, &allocation_info);

    void* mapped_data_ptr = nullptr;
    u64   mappped_data_size_bytes =
        alignOffset(allocation_info.size, backend.physical_device.properties.limits.nonCoherentAtomSize);

    AssertVk(vkMapMemory(backend.device, allocation_info.deviceMemory, allocation_info.offset, mappped_data_size_bytes,
                         0, &mapped_data_ptr));

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
    constexpr VkIndexType get_vk_meshlet_index_type()
    {
        if constexpr (IndexSizeBytes == 1)
            return VK_INDEX_TYPE_UINT8_EXT;
        else if constexpr (IndexSizeBytes == 2)
            return VK_INDEX_TYPE_UINT16;
        else if constexpr (IndexSizeBytes == 4)
            return VK_INDEX_TYPE_UINT32;
        else
        {
            AssertUnreachable();
        }
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
