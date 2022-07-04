////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"

namespace Reaper
{
struct LightingPassResources
{
    VkDescriptorSetLayout tile_depth_descriptor_set_layout;
    VkPipelineLayout      tile_depth_pipeline_layout;
    VkPipeline            tile_depth_pipeline;

    VkDescriptorSet tile_depth_descriptor_set;

    BufferInfo pointLightBuffer;
};

struct ReaperRoot;
struct VulkanBackend;

LightingPassResources create_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend);
void                  destroy_lighting_pass_resources(VulkanBackend& backend, LightingPassResources& resources);

struct SamplerResources;

void update_lighting_pass_pass_descriptor_set(VulkanBackend& backend, const LightingPassResources& resources,
                                              const SamplerResources& sampler_resources, VkImageView scene_depth_view,
                                              VkImageView tile_depth_min_view, VkImageView tile_depth_max_view);

struct PreparedData;

void upload_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                          LightingPassResources& resources);

struct CommandBuffer;

void record_tile_depth_pass_command_buffer(CommandBuffer& cmdBuffer, const LightingPassResources& pass_resources,
                                           VkExtent2D backbufferExtent);
} // namespace Reaper
