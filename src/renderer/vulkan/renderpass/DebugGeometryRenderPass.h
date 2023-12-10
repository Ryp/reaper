////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/vulkan/Buffer.h"

#include <vector>

namespace Reaper
{
static constexpr u32 DebugGeometryCountMax = 2048;

struct DebugMeshAlloc
{
    u32 index_offset;
    u32 index_count;
    u32 vertex_offset;
    u32 vertex_count;
};

struct DebugGeometryPassResources
{
    struct BuildCmds
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;
    } build_cmds;

    VkDescriptorSet build_cmds_descriptor_set;

    struct Draw
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;
    } draw;

    VkDescriptorSet draw_descriptor_set;

    GPUBuffer cpu_commands_staging_buffer;
    GPUBuffer build_cmds_constants;

    std::vector<DebugMeshAlloc> proxy_mesh_allocs;
    u32                         index_buffer_offset;
    GPUBuffer                   index_buffer;
    u32                         vertex_buffer_offset;
    GPUBuffer                   vertex_buffer_position;
};

struct VulkanBackend;
struct PipelineFactory;

DebugGeometryPassResources create_debug_geometry_pass_resources(VulkanBackend&   backend,
                                                                PipelineFactory& pipeline_factory);
void destroy_debug_geometry_pass_resources(VulkanBackend& backend, DebugGeometryPassResources& resources);

namespace FrameGraph
{
    class Builder;
    class FrameGraph;
} // namespace FrameGraph

struct DebugGeometryStartFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle draw_counter;
    FrameGraph::ResourceUsageHandle user_commands_buffer;
};

DebugGeometryStartFrameGraphRecord create_debug_geometry_start_pass_record(FrameGraph::Builder& builder);

struct DebugGeometryComputeFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle draw_counter;
    FrameGraph::ResourceUsageHandle user_commands_buffer;
    FrameGraph::ResourceUsageHandle draw_commands;
    FrameGraph::ResourceUsageHandle instance_buffer;
};

DebugGeometryComputeFrameGraphRecord
create_debug_geometry_compute_pass_record(FrameGraph::Builder&            builder,
                                          FrameGraph::ResourceUsageHandle draw_counter_handle,
                                          FrameGraph::ResourceUsageHandle user_commands_buffer_handle);

struct DebugGeometryDrawFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle scene_hdr;
    FrameGraph::ResourceUsageHandle scene_depth;
    FrameGraph::ResourceUsageHandle draw_counter;
    FrameGraph::ResourceUsageHandle draw_commands;
    FrameGraph::ResourceUsageHandle instance_buffer;
};

DebugGeometryDrawFrameGraphRecord
create_debug_geometry_draw_pass_record(FrameGraph::Builder&                        builder,
                                       const DebugGeometryComputeFrameGraphRecord& debug_geometry_build_cmds,
                                       FrameGraph::ResourceUsageHandle             draw_counter_handle,
                                       FrameGraph::ResourceUsageHandle             scene_hdr_usage_handle,
                                       FrameGraph::ResourceUsageHandle             scene_depth_usage_handle);

class DescriptorWriteHelper;
struct FrameGraphResources;
struct PreparedData;

void update_debug_geometry_start_resources(VulkanBackend& backend, const PreparedData& prepared,
                                           const DebugGeometryPassResources& resources);

void update_debug_geometry_build_cmds_pass_resources(VulkanBackend& backend, const FrameGraph::FrameGraph& frame_graph,
                                                     const FrameGraphResources&                  frame_graph_resources,
                                                     const DebugGeometryComputeFrameGraphRecord& record,
                                                     DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                                     const DebugGeometryPassResources& resources);

void update_debug_geometry_draw_pass_descriptor_sets(const FrameGraph::FrameGraph&            frame_graph,
                                                     const FrameGraphResources&               frame_graph_resources,
                                                     const DebugGeometryDrawFrameGraphRecord& record,
                                                     DescriptorWriteHelper&                   write_helper,
                                                     const DebugGeometryPassResources&        resources);

struct CommandBuffer;
struct FrameGraphHelper;

void record_debug_geometry_start_command_buffer(const FrameGraphHelper&                   frame_graph_helper,
                                                const DebugGeometryStartFrameGraphRecord& pass_record,
                                                CommandBuffer&                            cmdBuffer,
                                                const PreparedData&                       prepared,
                                                const DebugGeometryPassResources&         resources);

void record_debug_geometry_build_cmds_command_buffer(const FrameGraphHelper&                     frame_graph_helper,
                                                     const DebugGeometryComputeFrameGraphRecord& pass_record,
                                                     CommandBuffer&                              cmdBuffer,
                                                     const PipelineFactory&                      pipeline_factory,
                                                     const DebugGeometryPassResources&           resources);

void record_debug_geometry_draw_command_buffer(const FrameGraphHelper&                  frame_graph_helper,
                                               const DebugGeometryDrawFrameGraphRecord& pass_record,
                                               CommandBuffer&                           cmdBuffer,
                                               const PipelineFactory&                   pipeline_factory,
                                               const DebugGeometryPassResources&        resources);
} // namespace Reaper
