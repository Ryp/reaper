////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GuiPass.h"

#include "Frame.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/ShaderModules.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <backends/imgui_impl_vulkan.h>

namespace Reaper
{
GuiPassResources create_gui_pass_resources(ReaperRoot& /*root*/, VulkanBackend& backend,
                                           const ShaderModules& shader_modules)
{
    GuiPassResources resources = {};

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
         shader_modules.fullscreen_triangle_vs, default_entry_point(), nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
         shader_modules.gui_write_fs, default_entry_point(), nullptr}};

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    VkDescriptorSetLayout descriptor_set_layout =
        create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

    VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptor_set_layout, 1));

    VkPipelineColorBlendAttachmentState blend_attachment_state = default_pipeline_color_blend_attachment_state();

    const VkFormat gui_format = PixelFormatToVulkan(GUIFormat);

    GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
    pipeline_properties.blend_state.attachmentCount = 1;
    pipeline_properties.blend_state.pAttachments = &blend_attachment_state;
    pipeline_properties.pipeline_layout = pipelineLayout;
    pipeline_properties.pipeline_rendering.colorAttachmentCount = 1;
    pipeline_properties.pipeline_rendering.pColorAttachmentFormats = &gui_format;

    VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

    Assert(backend.physicalDeviceInfo.graphicsQueueFamilyIndex == backend.physicalDeviceInfo.presentQueueFamilyIndex);

    resources.guiPipe = GuiPipelineInfo{pipeline, pipelineLayout, descriptor_set_layout};

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             nonstd::span(&resources.guiPipe.descSetLayout, 1),
                             nonstd::span(&resources.descriptor_set, 1));

    return resources;
}

void destroy_gui_pass_resources(VulkanBackend& backend, GuiPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.guiPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.guiPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.guiPipe.descSetLayout, nullptr);
}

void record_gui_command_buffer(CommandBuffer& cmdBuffer, const GuiPassResources& pass_resources,
                               VkExtent2D backbufferExtent, VkImageView guiBufferView, ImDrawData* imgui_draw_data)
{
    const VkRect2D   pass_rect = default_vk_rect(backbufferExtent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.guiPipe.pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo color_attachment =
        default_rendering_attachment_info(guiBufferView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.clearValue = VkClearColor({0.f, 0.f, 0.f, 0.f});

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, &color_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.guiPipe.pipelineLayout, 0,
                            1, &pass_resources.descriptor_set, 0, nullptr);

    vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

    ImGui_ImplVulkan_RenderDrawData(imgui_draw_data, cmdBuffer.handle);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
