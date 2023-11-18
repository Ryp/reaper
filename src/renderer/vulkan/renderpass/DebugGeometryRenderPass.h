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
    VkDescriptorSetLayout build_cmds_descriptor_set_layout;
    VkPipelineLayout      build_cmds_pipeline_layout;
    VkPipeline            build_cmds_pipeline;

    VkDescriptorSet build_cmds_descriptor_set;

    VkDescriptorSetLayout draw_descriptor_set_layout;
    VkPipelineLayout      draw_pipeline_layout;
    VkPipeline            draw_pipeline;

    VkDescriptorSet draw_descriptor_set;

    GPUBuffer build_cmds_constants;

    std::vector<DebugMeshAlloc> proxy_mesh_allocs;
    u32                         index_buffer_offset;
    GPUBuffer                   index_buffer;
    u32                         vertex_buffer_offset;
    GPUBuffer                   vertex_buffer_position;
};

struct VulkanBackend;
struct ShaderModules;

DebugGeometryPassResources create_debug_geometry_pass_resources(VulkanBackend&       backend,
                                                                const ShaderModules& shader_modules);
void destroy_debug_geometry_pass_resources(VulkanBackend& backend, DebugGeometryPassResources& resources);

namespace FrameGraph
{
    class Builder;
    class FrameGraph;
} // namespace FrameGraph

struct DebugGeometryClearFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle draw_counter;
    FrameGraph::ResourceUsageHandle user_commands_buffer;
};

DebugGeometryClearFrameGraphRecord create_debug_geometry_clear_pass_record(FrameGraph::Builder& builder);

struct DebugGeometryComputeFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle draw_counter;
    FrameGraph::ResourceUsageHandle user_commands_buffer;
    FrameGraph::ResourceUsageHandle draw_commands;
    FrameGraph::ResourceUsageHandle instance_buffer;
};

DebugGeometryComputeFrameGraphRecord
create_debug_geometry_compute_pass_record(FrameGraph::Builder&                      builder,
                                          const DebugGeometryClearFrameGraphRecord& debug_geometry_clear);

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
                                       const DebugGeometryClearFrameGraphRecord&   debug_geometry_clear,
                                       const DebugGeometryComputeFrameGraphRecord& debug_geometry_build_cmds,
                                       FrameGraph::ResourceUsageHandle             scene_hdr_usage_handle,
                                       FrameGraph::ResourceUsageHandle             scene_depth_usage_handle);

struct PreparedData;

void upload_debug_geometry_build_cmds_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                                           const DebugGeometryPassResources& resources);

class DescriptorWriteHelper;
struct FrameGraphBuffer;
struct FrameGraphResources;

void update_debug_geometry_build_cmds_pass_descriptor_sets(const FrameGraph::FrameGraph& frame_graph,
                                                           const FrameGraphResources&    frame_graph_resources,
                                                           const DebugGeometryComputeFrameGraphRecord& record,
                                                           DescriptorWriteHelper&                      write_helper,
                                                           const DebugGeometryPassResources&           resources);

struct CommandBuffer;
struct FrameGraphTexture;

void record_debug_geometry_build_cmds_command_buffer(CommandBuffer&                    cmdBuffer,
                                                     const DebugGeometryPassResources& resources);

void update_debug_geometry_draw_pass_descriptor_sets(const FrameGraph::FrameGraph&            frame_graph,
                                                     const FrameGraphResources&               frame_graph_resources,
                                                     const DebugGeometryDrawFrameGraphRecord& record,
                                                     DescriptorWriteHelper&                   write_helper,
                                                     const DebugGeometryPassResources&        resources);

void record_debug_geometry_draw_command_buffer(CommandBuffer& cmdBuffer, const DebugGeometryPassResources& resources,
                                               const FrameGraphTexture& hdr_buffer,
                                               const FrameGraphTexture& depth_texture,
                                               const FrameGraphBuffer&  draw_counter,
                                               const FrameGraphBuffer&  draw_commands);
} // namespace Reaper
