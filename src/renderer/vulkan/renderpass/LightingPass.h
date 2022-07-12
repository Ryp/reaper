////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"

#include <array>

namespace Reaper
{
struct LightingPassResources
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

    BufferInfo pointLightBuffer;
    BufferInfo lightVolumeBuffer;
    BufferInfo light_list_buffer;
};

struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

LightingPassResources create_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                     const ShaderModules& shader_modules);
void                  destroy_lighting_pass_resources(VulkanBackend& backend, LightingPassResources& resources);

struct SamplerResources;

void update_lighting_pass_descriptor_set(VulkanBackend& backend, const LightingPassResources& resources,
                                         const SamplerResources& sampler_resources, VkImageView scene_depth_view,
                                         VkImageView tile_depth_min_view, VkImageView tile_depth_max_view);

void update_depth_copy_pass_descriptor_set(VulkanBackend& backend, const LightingPassResources& resources,
                                           VkImageView depth_min_src, VkImageView depth_max_src);

void update_light_raster_pass_descriptor_sets(VulkanBackend& backend, const LightingPassResources& resources,
                                              VkImageView depth_min, VkImageView depth_max, VkBuffer light_list_buffer,
                                              const GPUBufferView& light_list_view);

struct PreparedData;

void upload_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                          LightingPassResources& resources);

struct CommandBuffer;

void record_tile_depth_pass_command_buffer(CommandBuffer& cmdBuffer, const LightingPassResources& pass_resources,
                                           VkExtent2D backbufferExtent);

void record_depth_copy(CommandBuffer& cmdBuffer, const LightingPassResources& pass_resources, VkImageView depth_min_dst,
                       VkImageView depth_max_dst, VkExtent2D backbufferExtent);

void record_light_raster_command_buffer(CommandBuffer& cmdBuffer, const LightingPassResources& resources,
                                        const PreparedData& prepared, VkImageView depth_min, VkImageView depth_max,
                                        VkExtent2D depth_extent);
} // namespace Reaper
