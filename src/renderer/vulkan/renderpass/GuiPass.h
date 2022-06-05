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
constexpr PixelFormat GUIFormat = PixelFormat::R8G8B8A8_SRGB;

struct GuiPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

struct GuiPassResources
{
    GuiPipelineInfo guiPipe;

    VkDescriptorSet descriptor_set;
};

GuiPassResources create_gui_pass_resources(ReaperRoot& root, VulkanBackend& backend);
void             destroy_gui_pass_resources(VulkanBackend& backend, GuiPassResources& resources);

struct CommandBuffer;
struct FrameData;

void record_gui_command_buffer(CommandBuffer& cmdBuffer, const GuiPassResources& pass_resources,
                               VkExtent2D backbufferExtent, VkImageView guiBufferView);
} // namespace Reaper
