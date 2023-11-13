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
constexpr u32 TiledRasterMaxIndirectCommandCount = 512;

struct ProxyMeshAlloc
{
    u32 vertex_offset;
    u32 vertex_count;
};

struct TiledRasterResources
{
    struct TileDepth
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        VkPipeline            pipeline;

        VkDescriptorSet descriptor_set;
    } tile_depth;

    struct DepthCopy
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        VkPipeline            pipeline;

        VkDescriptorSet descriptor_set;
    } depth_copy;

    struct Raster
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        VkPipeline            pipeline;

        std::array<VkDescriptorSet, 2> descriptor_sets;
    } light_raster;

    VkDescriptorSetLayout classify_descriptor_set_layout;
    VkPipelineLayout      classify_pipeline_layout;
    VkPipeline            classify_pipeline;

    VkDescriptorSet classify_descriptor_set;

    std::vector<ProxyMeshAlloc> proxy_mesh_allocs;
    u32                         vertex_buffer_offset;
    GPUBuffer                   vertex_buffer_position;
    GPUBuffer                   light_list_buffer;
};

struct VulkanBackend;
struct ShaderModules;

TiledRasterResources create_tiled_raster_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules);
void                 destroy_tiled_raster_pass_resources(VulkanBackend& backend, TiledRasterResources& resources);

struct SamplerResources;
struct GPUBufferView;
class DescriptorWriteHelper;
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
                                           const FrameGraphTexture&    hzb_texture);

void update_classify_descriptor_set(DescriptorWriteHelper&      write_helper,
                                    const TiledRasterResources& resources,
                                    const FrameGraphBuffer&     classification_counters,
                                    const FrameGraphBuffer&     draw_commands_inner,
                                    const FrameGraphBuffer&     draw_commands_outer);

void update_light_raster_pass_descriptor_sets(DescriptorWriteHelper&      write_helper,
                                              const TiledRasterResources& resources,
                                              const FrameGraphTexture&    depth_min,
                                              const FrameGraphTexture&    depth_max,
                                              const FrameGraphBuffer&     light_list_buffer);

struct SceneGraph;
struct TiledLightingFrame;

void prepare_tile_lighting_frame(const SceneGraph& scene, TiledLightingFrame& tiled_lighting_frame);

struct PreparedData;
struct StorageBufferAllocator;

void upload_tiled_raster_pass_frame_resources(DescriptorWriteHelper&    write_helper,
                                              StorageBufferAllocator&   frame_storage_allocator,
                                              const TiledLightingFrame& tiled_lighting_frame,
                                              TiledRasterResources&     resources);

struct CommandBuffer;

void record_tile_depth_pass_command_buffer(CommandBuffer&                         cmdBuffer,
                                           const TiledRasterResources::TileDepth& tile_depth_resources,
                                           VkExtent2D                             render_extent);

void record_depth_copy(CommandBuffer& cmdBuffer, const TiledRasterResources& pass_resources,
                       const FrameGraphTexture& depth_min_dst, const FrameGraphTexture& depth_max_dst);

void record_light_classify_command_buffer(CommandBuffer&              cmdBuffer,
                                          const TiledLightingFrame&   tiled_lighting_frame,
                                          const TiledRasterResources& resources);

void record_light_raster_command_buffer(CommandBuffer&                      cmdBuffer,
                                        const TiledRasterResources::Raster& light_raster_resources,
                                        const FrameGraphBuffer&             command_counters,
                                        const FrameGraphBuffer&             draw_commands_inner,
                                        const FrameGraphBuffer& draw_commands_outer, const FrameGraphTexture& depth_min,
                                        const FrameGraphTexture& depth_max);
} // namespace Reaper
