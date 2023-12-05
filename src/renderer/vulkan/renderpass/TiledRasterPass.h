////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/Buffer.h"

#include <array>
#include <vector>

namespace Reaper
{
constexpr u32 TiledRasterMaxIndirectCommandCount = 512;

struct ProxyMeshAlloc
{
    u32 vertex_offset;
    u32 vertex_count;
};

struct TiledRasterResources
{
    struct DepthCopy
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;

        VkDescriptorSet descriptor_set;
    } depth_copy;

    struct Raster
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;

        std::array<VkDescriptorSet, 2> descriptor_sets;
    } light_raster;

    struct Classify
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;

        VkDescriptorSet descriptor_set;
    } classify;

    std::vector<ProxyMeshAlloc> proxy_mesh_allocs;
    u32                         vertex_buffer_offset;
    GPUBuffer                   vertex_buffer_position;
    GPUBuffer                   light_list_buffer;
};

struct VulkanBackend;
struct PipelineFactory;

TiledRasterResources create_tiled_raster_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory);
void                 destroy_tiled_raster_pass_resources(VulkanBackend& backend, TiledRasterResources& resources);

namespace FrameGraph
{
    struct FrameGraph;
    struct Builder;
}; // namespace FrameGraph

struct LightRasterFrameGraphRecord
{
    GPUTextureProperties tile_depth_properties;

    struct TileDepthCopy
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle depth_min;
        FrameGraph::ResourceUsageHandle depth_max;
        FrameGraph::ResourceUsageHandle hzb_texture;
        FrameGraph::ResourceUsageHandle light_list_clear;
        FrameGraph::ResourceUsageHandle classification_counters_clear;
    } tile_depth_copy;

    struct Classify
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle classification_counters;
        FrameGraph::ResourceUsageHandle draw_commands_inner;
        FrameGraph::ResourceUsageHandle draw_commands_outer;
    } light_classify;

    struct Raster
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle command_counters;
        FrameGraph::ResourceUsageHandle draw_commands_inner;
        FrameGraph::ResourceUsageHandle draw_commands_outer;
        FrameGraph::ResourceUsageHandle tile_depth_min;
        FrameGraph::ResourceUsageHandle tile_depth_max;
        FrameGraph::ResourceUsageHandle light_list;
    } light_raster;
};

struct TiledLightingFrame;
struct GPUTextureProperties;

LightRasterFrameGraphRecord create_tiled_lighting_raster_pass_record(FrameGraph::Builder&        builder,
                                                                     const TiledLightingFrame&   tiled_lighting_frame,
                                                                     const GPUTextureProperties& hzb_properties,
                                                                     FrameGraph::ResourceUsageHandle hzb_usage_handle);

struct FrameGraphResources;
class DescriptorWriteHelper;
struct StorageBufferAllocator;

void update_tiled_lighting_raster_pass_resources(const FrameGraph::FrameGraph&      frame_graph,
                                                 const FrameGraphResources&         frame_graph_resources,
                                                 const LightRasterFrameGraphRecord& record,
                                                 DescriptorWriteHelper&             write_helper,
                                                 StorageBufferAllocator&            frame_storage_allocator,
                                                 const TiledRasterResources&        resources,
                                                 const TiledLightingFrame&          tiled_lighting_frame);

struct CommandBuffer;
struct FrameGraphHelper;

void record_depth_copy(const FrameGraphHelper&                           frame_graph_helper,
                       const LightRasterFrameGraphRecord::TileDepthCopy& pass_record, CommandBuffer& cmdBuffer,
                       const PipelineFactory& pipeline_factory, const TiledRasterResources& resources);

void record_light_classify_command_buffer(const FrameGraphHelper&                      frame_graph_helper,
                                          const LightRasterFrameGraphRecord::Classify& pass_record,
                                          CommandBuffer&                               cmdBuffer,
                                          const PipelineFactory&                       pipeline_factory,
                                          const TiledLightingFrame&                    tiled_lighting_frame,
                                          const TiledRasterResources&                  resources);

void record_light_raster_command_buffer(const FrameGraphHelper&                    frame_graph_helper,
                                        const LightRasterFrameGraphRecord::Raster& pass_record,
                                        CommandBuffer&                             cmdBuffer,
                                        const PipelineFactory&                     pipeline_factory,
                                        const TiledRasterResources::Raster&        light_raster_resources);
} // namespace Reaper
