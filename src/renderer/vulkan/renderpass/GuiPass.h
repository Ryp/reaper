////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
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

struct GuiPassResources
{
    u32                   pipeline_index;
    VkPipelineLayout      pipeline_layout;
    VkDescriptorSetLayout descriptor_set_layout;

    VkDescriptorSet descriptor_set;
};

struct VulkanBackend;
struct PipelineFactory;

GuiPassResources create_gui_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory);
void             destroy_gui_pass_resources(VulkanBackend& backend, GuiPassResources& resources);

namespace FrameGraph
{
    class Builder;
    struct FrameGraph;
} // namespace FrameGraph

struct GUIFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle output;
};

GUIFrameGraphRecord create_gui_pass_record(FrameGraph::Builder& builder, VkExtent2D gui_extent);

struct CommandBuffer;
struct FrameGraphHelper;

void record_gui_command_buffer(const FrameGraphHelper& frame_graph_helper, const GUIFrameGraphRecord& pass_record,
                               CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                               const GuiPassResources& pass_resources, ImDrawData* imgui_draw_data);
} // namespace Reaper
