////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"

#include <array>
#include <vector>

namespace Reaper
{
struct ProxyMeshAlloc
{
    u32 vertex_offset;
    u32 vertex_count;
};

struct TiledRasterResources
{
    VkDescriptorSetLayout tile_depth_descriptor_set_layout;
    VkPipelineLayout      tile_depth_pipeline_layout;
    VkPipeline            tile_depth_pipeline;

    VkDescriptorSet tile_depth_descriptor_set;

    VkDescriptorSetLayout depth_copy_descriptor_set_layout;
    VkPipelineLayout      depth_copy_pipeline_layout;
    VkPipeline            depth_copy_pipeline;

    std::array<VkDescriptorSet, 2> depth_copy_descriptor_sets;

    VkDescriptorSetLayout light_raster_descriptor_set_layout;
    VkPipelineLayout      light_raster_pipeline_layout;
    VkPipeline            light_raster_pipeline;

    std::array<VkDescriptorSet, 2> light_raster_descriptor_sets;

    VkDescriptorSetLayout tiled_lighting_descriptor_set_layout;
    VkDescriptorSetLayout tiled_lighting_descriptor_set_layout_material;
    VkPipelineLayout      tiled_lighting_pipeline_layout;
    VkPipeline            tiled_lighting_pipeline;

    VkDescriptorSet tiled_lighting_descriptor_set;
    VkDescriptorSet tiled_lighting_descriptor_set_material;

    std::vector<ProxyMeshAlloc> proxy_allocs;
    u32                         vertex_buffer_offset;
    BufferInfo                  vertex_buffer_position;
    BufferInfo                  light_volume_buffer;
    BufferInfo                  light_list_buffer;
};

struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

TiledRasterResources create_tiled_raster_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                        const ShaderModules& shader_modules);
void                 destroy_tiled_raster_pass_resources(VulkanBackend& backend, TiledRasterResources& resources);

struct SamplerResources;
struct GPUBufferView;
struct DescriptorWriteHelper;
struct FrameGraphTexture;
struct FrameGraphBuffer;
struct LightingPassResources;

void update_lighting_depth_downsample_descriptor_set(DescriptorWriteHelper&      write_helper,
                                                     const TiledRasterResources& resources,
                                                     const SamplerResources&     sampler_resources,
                                                     const FrameGraphTexture&    scene_depth,
                                                     const FrameGraphTexture&    tile_depth_min,
                                                     const FrameGraphTexture&    tile_depth_max);

void update_depth_copy_pass_descriptor_set(DescriptorWriteHelper&      write_helper,
                                           const TiledRasterResources& resources,
                                           const FrameGraphTexture&    depth_min_src,
                                           const FrameGraphTexture&    depth_max_src);

void update_light_raster_pass_descriptor_sets(DescriptorWriteHelper&      write_helper,
                                              const TiledRasterResources& resources,
                                              const FrameGraphTexture&    depth_min,
                                              const FrameGraphTexture&    depth_max,
                                              const FrameGraphBuffer&     light_list_buffer);

struct PreparedData;

void upload_tiled_raster_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                              TiledRasterResources& resources);

struct CommandBuffer;

void record_tile_depth_pass_command_buffer(CommandBuffer& cmdBuffer, const TiledRasterResources& pass_resources,
                                           VkExtent2D backbufferExtent);

void record_depth_copy(CommandBuffer& cmdBuffer, const TiledRasterResources& pass_resources,
                       const FrameGraphTexture& depth_min_dst, const FrameGraphTexture& depth_max_dst);

void record_light_raster_command_buffer(CommandBuffer& cmdBuffer, const TiledRasterResources& resources,
                                        const PreparedData& prepared, const FrameGraphTexture& depth_min,
                                        const FrameGraphTexture& depth_max);
} // namespace Reaper