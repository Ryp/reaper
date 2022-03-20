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
struct GuiPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

GuiPipelineInfo create_gui_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass);

struct GuiPassResources
{
    VkRenderPass    guiRenderPass;
    GuiPipelineInfo guiPipe;

    GPUTextureProperties swapchain_properties; // FIXME
    VkFramebuffer        gui_framebuffer;

    ImageInfo   guiTexture;
    VkImageView guiTextureView;

    VkDescriptorSet descriptor_set;
};

GuiPassResources create_gui_pass_resources(ReaperRoot& root, VulkanBackend& backend, glm::uvec2 extent);
void             destroy_gui_pass_resources(VulkanBackend& backend, const GuiPassResources& resources);

void resize_gui_pass_resources(ReaperRoot& root, VulkanBackend& backend, GuiPassResources& resources,
                               glm::uvec2 extent);

struct CommandBuffer;
struct FrameData;

void record_gui_command_buffer(CommandBuffer& cmdBuffer, const GuiPassResources& pass_resources);
} // namespace Reaper
