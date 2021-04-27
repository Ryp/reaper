////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/api/Vulkan.h"

#include <glm/vec2.hpp>

#include <nonstd/span.hpp>

#include <vector>

namespace Reaper
{
struct SwapchainPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

SwapchainPipelineInfo create_swapchain_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass);

struct SwapchainPassResources
{
    BufferInfo passConstantBuffer;

    VkRenderPass swapchainRenderPass;

    SwapchainPipelineInfo swapchainPipe;

    GPUTextureProperties swapchain_properties; // FIXME
    VkFramebuffer        swapchain_framebuffer;

    VkSampler shadowMapSampler;

    struct Frame
    {
        VkDescriptorSet descriptor_set;
    } frame;
};

SwapchainPassResources create_swapchain_pass_resources(ReaperRoot& root, VulkanBackend& backend, glm::uvec2 extent);
void                   destroy_swapchain_pass_resources(VulkanBackend& backend, SwapchainPassResources& resources);

void resize_swapchain_pass_resources(ReaperRoot& root, VulkanBackend& backend, SwapchainPassResources& resources,
                                     glm::uvec2 extent);

VkDescriptorSet create_swapchain_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                     const SwapchainPassResources& resources, VkImageView texture_view);

void prepare_swapchain_frame_resources(ReaperRoot& root, VulkanBackend& backend, SwapchainPassResources& pass_resources,
                                       VkImageView hdr_buffer_view);

struct FrameData;

void record_swapchain_command_buffer(VkCommandBuffer cmdBuffer, const FrameData& frame_data,
                                     const SwapchainPassResources& pass_resources, VkImageView swapchain_buffer_view);
} // namespace Reaper
