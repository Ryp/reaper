////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GuiPass.h"

#include "FrameGraphPass.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/ShaderModules.h"

#include <backends/imgui_impl_vulkan.h>

namespace Reaper
{
GuiPassResources create_gui_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules)
{
    GuiPassResources resources = {};

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
        default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shader_modules.fullscreen_triangle_vs),
        default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shader_modules.gui_write_fs),
    };

    // Nothing for now
    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {};

    VkDescriptorSetLayout descriptor_set_layout =
        create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

    VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1));

    VkPipelineColorBlendAttachmentState blend_attachment_state = default_pipeline_color_blend_attachment_state();

    const VkFormat gui_format = PixelFormatToVulkan(GUIFormat);

    GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
    pipeline_properties.blend_state.attachmentCount = 1;
    pipeline_properties.blend_state.pAttachments = &blend_attachment_state;
    pipeline_properties.pipeline_layout = pipelineLayout;
    pipeline_properties.pipeline_rendering.colorAttachmentCount = 1;
    pipeline_properties.pipeline_rendering.pColorAttachmentFormats = &gui_format;

    VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

    resources.guiPipe = GuiPipelineInfo{pipeline, pipelineLayout, descriptor_set_layout};

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.guiPipe.descSetLayout, 1), std::span(&resources.descriptor_set, 1));

    return resources;
}

void destroy_gui_pass_resources(VulkanBackend& backend, GuiPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.guiPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.guiPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.guiPipe.descSetLayout, nullptr);
}

GUIFrameGraphRecord create_gui_pass_record(FrameGraph::Builder& builder, VkExtent2D gui_extent)
{
    GUIFrameGraphRecord gui;
    gui.pass_handle = builder.create_render_pass("GUI");

    gui.output = builder.create_texture(
        gui.pass_handle, "GUI SDR",
        default_texture_properties(gui_extent.width, gui_extent.height, GUIFormat,
                                   GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled),
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});

    return gui;
}

void record_gui_command_buffer(const FrameGraphHelper& frame_graph_helper, const GUIFrameGraphRecord& pass_record,
                               CommandBuffer& cmdBuffer, const GuiPassResources& pass_resources,
                               ImDrawData* imgui_draw_data)
{
    REAPER_GPU_SCOPE(cmdBuffer, "GUI");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const FrameGraphTexture gui_buffer =
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.output);

    const VkExtent2D gui_extent = {gui_buffer.properties.width, gui_buffer.properties.height};
    const VkRect2D   pass_rect = default_vk_rect(gui_extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.guiPipe.pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo color_attachment =
        default_rendering_attachment_info(gui_buffer.default_view_handle, gui_buffer.image_layout);
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.clearValue = VkClearColor({0.f, 0.f, 0.f, 0.f});

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, &color_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.guiPipe.pipelineLayout, 0,
                            1, &pass_resources.descriptor_set, 0, nullptr);

    // vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

    ImGui_ImplVulkan_RenderDrawData(imgui_draw_data, cmdBuffer.handle);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
