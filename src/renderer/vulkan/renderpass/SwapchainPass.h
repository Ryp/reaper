////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"
#include <vulkan_loader/Vulkan.h>

#include <glm/vec2.hpp>

#include <span>

#include <vector>

namespace Reaper
{
struct SwapchainPassResources
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;

    VkDescriptorSet descriptor_set;
};

struct VulkanBackend;
struct ShaderModules;

SwapchainPassResources create_swapchain_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules);
void destroy_swapchain_pass_resources(VulkanBackend& backend, const SwapchainPassResources& resources);

void reload_swapchain_pipeline(VulkanBackend& backend, const ShaderModules& shader_modules,
                               SwapchainPassResources& resources);

struct SamplerResources;
class DescriptorWriteHelper;
struct FrameGraphTexture;

void update_swapchain_pass_descriptor_set(DescriptorWriteHelper& write_helper, const SwapchainPassResources& resources,
                                          const SamplerResources&  sampler_resources,
                                          const FrameGraphTexture& hdr_scene_texture,
                                          const FrameGraphTexture& lighting_texture,
                                          const FrameGraphTexture& gui_texture,
                                          const FrameGraphTexture& tile_lighting_debug_texture);

struct CommandBuffer;

void record_swapchain_command_buffer(CommandBuffer& cmdBuffer, const SwapchainPassResources& pass_resources,
                                     VkImageView swapchain_buffer_view, VkExtent2D swapchain_extent);
} // namespace Reaper
