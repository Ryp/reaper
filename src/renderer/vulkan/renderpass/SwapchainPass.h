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

#include <nonstd/span.hpp>

#include <vector>

namespace Reaper
{
struct SwapchainPassResources
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;

    BufferInfo passConstantBuffer;

    VkDescriptorSet descriptor_set;
};

struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

SwapchainPassResources create_swapchain_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                       const ShaderModules& shader_modules);
void destroy_swapchain_pass_resources(VulkanBackend& backend, const SwapchainPassResources& resources);

void reload_swapchain_pipeline(VulkanBackend& backend, const ShaderModules& shader_modules,
                               SwapchainPassResources& resources);

struct SamplerResources;

void update_swapchain_pass_descriptor_set(VulkanBackend& backend, const SwapchainPassResources& resources,
                                          const SamplerResources& sampler_resources, VkImageView hdr_scene_texture_view,
                                          VkImageView gui_texture_view);

struct PreparedData;

void upload_swapchain_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                      const SwapchainPassResources& pass_resources);

struct CommandBuffer;
struct FrameData;

void record_swapchain_command_buffer(CommandBuffer& cmdBuffer, const FrameData& frame_data,
                                     const SwapchainPassResources& pass_resources, VkImageView swapchain_buffer_view);
} // namespace Reaper
