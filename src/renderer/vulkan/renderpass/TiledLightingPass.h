////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/vulkan/Buffer.h"

#include <span>
#include <vector>

namespace Reaper
{
struct TiledLightingPassResources
{
    struct Lighting
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;
    } lighting;

    VkDescriptorSet tiled_lighting_descriptor_set;

    GPUBuffer tiled_lighting_constant_buffer;

    // Debug
    struct LightingDebug
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;
    } debug;

    VkDescriptorSet debug_descriptor_set;
};

struct VulkanBackend;
struct PipelineFactory;

TiledLightingPassResources create_tiled_lighting_pass_resources(VulkanBackend&   backend,
                                                                PipelineFactory& pipeline_factory);
void destroy_tiled_lighting_pass_resources(VulkanBackend& backend, TiledLightingPassResources& resources);

namespace FrameGraph
{
    class Builder;
    class FrameGraph;
} // namespace FrameGraph

struct TiledLightingFrameGraphRecord
{
    FrameGraph::RenderPassHandle                 pass_handle;
    std::vector<FrameGraph::ResourceUsageHandle> shadow_maps;
    FrameGraph::ResourceUsageHandle              light_list;
    FrameGraph::ResourceUsageHandle              depth;
    FrameGraph::ResourceUsageHandle              gbuffer_rt0;
    FrameGraph::ResourceUsageHandle              gbuffer_rt1;
    FrameGraph::ResourceUsageHandle              lighting;
    FrameGraph::ResourceUsageHandle              tile_debug_texture;
};

struct VisBufferFrameGraphRecord;
struct ShadowFrameGraphRecord;
struct LightRasterFrameGraphRecord;

TiledLightingFrameGraphRecord create_tiled_lighting_pass_record(FrameGraph::Builder&               builder,
                                                                const VisBufferFrameGraphRecord&   vis_buffer_record,
                                                                const ShadowFrameGraphRecord&      shadow,
                                                                const LightRasterFrameGraphRecord& light_raster_record);

struct TiledLightingDebugFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle tile_debug;
    FrameGraph::ResourceUsageHandle output;
};

TiledLightingDebugFrameGraphRecord create_tiled_lighting_debug_pass_record(
    FrameGraph::Builder& builder, const TiledLightingFrameGraphRecord& tiled_lighting_record, VkExtent2D render_extent);

struct FrameGraphResources;
class DescriptorWriteHelper;
struct LightingPassResources;
struct PreparedData;
struct SamplerResources;

void update_tiled_lighting_pass_resources(VulkanBackend& backend, const FrameGraph::FrameGraph& frame_graph,
                                          const FrameGraphResources&           frame_graph_resources,
                                          const TiledLightingFrameGraphRecord& record,
                                          DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                          const LightingPassResources&      lighting_resources,
                                          const TiledLightingPassResources& resources,
                                          const SamplerResources&           sampler_resources);

struct CommandBuffer;
struct FrameGraphHelper;

void record_tiled_lighting_command_buffer(const FrameGraphHelper&              frame_graph_helper,
                                          const TiledLightingFrameGraphRecord& pass_record, CommandBuffer& cmdBuffer,
                                          const PipelineFactory&            pipeline_factory,
                                          const TiledLightingPassResources& resources, VkExtent2D render_extent,
                                          VkExtent2D tile_extent);

void update_tiled_lighting_debug_pass_resources(const FrameGraph::FrameGraph&             frame_graph,
                                                const FrameGraphResources&                frame_graph_resources,
                                                const TiledLightingDebugFrameGraphRecord& record,
                                                DescriptorWriteHelper&                    write_helper,
                                                const TiledLightingPassResources&         resources);

void record_tiled_lighting_debug_command_buffer(const FrameGraphHelper&                   frame_graph_helper,
                                                const TiledLightingDebugFrameGraphRecord& pass_record,
                                                CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                                const TiledLightingPassResources& resources, VkExtent2D render_extent,
                                                VkExtent2D tile_extent);
} // namespace Reaper
