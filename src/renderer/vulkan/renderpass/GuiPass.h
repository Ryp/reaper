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

struct ImDrawData;

namespace Reaper
{
constexpr PixelFormat GUIFormat = PixelFormat::R8G8B8A8_SRGB;

struct GuiPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct VulkanBackend;
struct GPUTextureProperties;
struct ShaderModules;

struct GuiPassResources
{
    GuiPipelineInfo guiPipe;

    VkDescriptorSet descriptor_set;
};

GuiPassResources create_gui_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules);
void             destroy_gui_pass_resources(VulkanBackend& backend, GuiPassResources& resources);

struct CommandBuffer;
struct FrameGraphTexture;

void record_gui_command_buffer(CommandBuffer& cmdBuffer, const GuiPassResources& pass_resources,
                               const FrameGraphTexture& gui_buffer, ImDrawData* imgui_draw_data);
} // namespace Reaper
