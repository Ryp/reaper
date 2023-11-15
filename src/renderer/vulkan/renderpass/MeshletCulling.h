////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/vulkan/Buffer.h"

#include <vulkan_loader/Vulkan.h>

#include <array>
#include <vector>

namespace Reaper
{
namespace FrameGraph
{
    class FrameGraph;
}
struct FrameGraphResources;

// FIXME
struct SimplePipeline
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct MeshletCullingResources
{
    SimplePipeline cull_meshlets_pipe;
    SimplePipeline cull_meshlets_prep_indirect_pipe;
    SimplePipeline cull_triangles_pipe;

    std::array<VkDescriptorSet, 4> cull_meshlet_descriptor_sets;
    std::array<VkDescriptorSet, 4> cull_triangles_descriptor_sets;
    VkDescriptorSet                cull_prepare_descriptor_set;

    GPUBuffer           counters_cpu_buffer;
    GPUBufferProperties counters_cpu_properties;

    VkEvent countersReadyEvent;
};

struct CullMeshletsFrameGraphRecord
{
    struct Clear
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle meshlet_counters;
    } clear;

    struct CullMeshlets
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle meshlet_counters;
        FrameGraph::ResourceUsageHandle visible_meshlet_offsets;
    } cull_meshlets;

    struct CullTrianglesPrepare
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle meshlet_counters;
        FrameGraph::ResourceUsageHandle indirect_dispatch_buffer;
    } cull_triangles_prepare;

    struct CullTriangles
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle indirect_dispatch_buffer;
        FrameGraph::ResourceUsageHandle meshlet_counters;
        FrameGraph::ResourceUsageHandle visible_meshlet_offsets;
        FrameGraph::ResourceUsageHandle meshlet_indirect_draw_commands;
        FrameGraph::ResourceUsageHandle meshlet_visible_index_buffer;
        FrameGraph::ResourceUsageHandle visible_meshlet_buffer;
    } cull_triangles;

    struct Debug
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle meshlet_counters;
    } debug;
};

namespace FrameGraph
{
    class Builder;
}

CullMeshletsFrameGraphRecord create_cull_meshlet_frame_graph_record(FrameGraph::Builder& builder);

struct VulkanBackend;
struct ShaderModules;

MeshletCullingResources create_meshlet_culling_resources(VulkanBackend& backend, const ShaderModules& shader_modules);
void                    destroy_meshlet_culling_resources(VulkanBackend& backend, MeshletCullingResources& resources);

class DescriptorWriteHelper;
struct StorageBufferAllocator;
struct PreparedData;
struct MeshCache;

void update_meshlet_culling_passes_resources(const FrameGraph::FrameGraph&       frame_graph,
                                             FrameGraphResources&                frame_graph_resources,
                                             const CullMeshletsFrameGraphRecord& record,
                                             DescriptorWriteHelper&              write_helper,
                                             StorageBufferAllocator&             frame_storage_allocator,
                                             const PreparedData&                 prepared,
                                             MeshletCullingResources&            resources,
                                             const MeshCache&                    mesh_cache);

struct ReaperRoot;
struct CommandBuffer;

void record_meshlet_culling_command_buffer(ReaperRoot& root, CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                           MeshletCullingResources& resources);

void record_triangle_culling_prepare_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                                    MeshletCullingResources& resources);

struct FrameGraphBuffer;

void record_triangle_culling_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                            MeshletCullingResources& resources,
                                            const FrameGraphBuffer&  indirect_dispatch_buffer);

void record_meshlet_culling_debug_command_buffer(CommandBuffer&           cmdBuffer,
                                                 MeshletCullingResources& resources,
                                                 const FrameGraphBuffer&  meshlet_counters);

struct MeshletCullingStats
{
    u32 pass_index;
    u32 surviving_meshlet_count;
    u32 surviving_triangle_count;
    u32 indirect_draw_command_count;
};

std::vector<MeshletCullingStats> get_meshlet_culling_gpu_stats(VulkanBackend& backend, const PreparedData& prepared,
                                                               MeshletCullingResources& resources);

struct MeshletDrawParams
{
    u64         counter_buffer_offset;
    u64         index_buffer_offset;
    VkIndexType index_type;
    u64         command_buffer_offset;
    u32         command_buffer_max_count;
};

MeshletDrawParams get_meshlet_draw_params(u32 pass_index);
BufferSubresource get_meshlet_visible_index_buffer_pass(u32 pass_index);
} // namespace Reaper
