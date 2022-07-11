////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "vulkan_loader/Vulkan.h"

#include <nonstd/span.hpp>

namespace Reaper
{
void allocate_descriptor_sets(VkDevice device, VkDescriptorPool descriptor_pool,
                              nonstd::span<const VkDescriptorSetLayout> descriptor_set_layouts,
                              nonstd::span<VkDescriptorSet>             output_descriptor_sets);

void allocate_descriptor_sets(VkDevice device, VkDescriptorPool descriptor_pool, VkDescriptorSetLayout set_layout,
                              nonstd::span<VkDescriptorSet> output_descriptor_sets);

VkDescriptorSetLayoutBindingFlagsCreateInfo
descriptor_set_layout_binding_flags_create_info(nonstd::span<const VkDescriptorBindingFlags> binding_flags);

VkDescriptorSetLayoutCreateInfo
descriptor_set_layout_create_info(nonstd::span<const VkDescriptorSetLayoutBinding> layout_bindings);

VkDescriptorSetLayout create_descriptor_set_layout(VkDevice                                         device,
                                                   nonstd::span<const VkDescriptorSetLayoutBinding> layout_bindings);

const char* default_entry_point();

VkPipelineLayout create_pipeline_layout(
    VkDevice device, nonstd::span<const VkDescriptorSetLayout> descriptor_set_layouts,
    nonstd::span<const VkPushConstantRange> push_constant_ranges = nonstd::span<const VkPushConstantRange>());

VkPipeline create_compute_pipeline(VkDevice device, VkPipelineLayout pipeline_layout, VkShaderModule compute_shader,
                                   VkSpecializationInfo* specialization_info = nullptr);

VkPipelineColorBlendAttachmentState default_pipeline_color_blend_attachment_state();
VkPipelineRenderingCreateInfo       default_pipeline_rendering_create_info();

VkPipelineVertexInputStateCreateInfo   default_pipeline_vertex_input_state_create_info();
VkPipelineInputAssemblyStateCreateInfo default_pipeline_input_assembly_state_create_info();
VkPipelineViewportStateCreateInfo      default_pipeline_viewport_state_create_info();
VkPipelineRasterizationStateCreateInfo default_pipeline_rasterization_state_create_info();
VkPipelineMultisampleStateCreateInfo   default_pipeline_multisample_state_create_info();
VkPipelineDepthStencilStateCreateInfo  default_pipeline_depth_stencil_state_create_info();
VkPipelineColorBlendStateCreateInfo    default_pipeline_color_blend_state_create_info();

VkPipelineDynamicStateCreateInfo
create_pipeline_dynamic_state_create_info(nonstd::span<const VkDynamicState> dynamic_states);

// Helper with the mandatory structures you need to have to create a graphics pipeline.
struct GraphicsPipelineProperties
{
    VkPipelineVertexInputStateCreateInfo   vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineViewportStateCreateInfo      viewport;
    VkPipelineRasterizationStateCreateInfo raster;
    VkPipelineMultisampleStateCreateInfo   multisample;
    VkPipelineDepthStencilStateCreateInfo  depth_stencil;
    VkPipelineColorBlendStateCreateInfo    blend_state;
    VkPipelineLayout                       pipeline_layout;
    VkPipelineRenderingCreateInfo          pipeline_rendering; // Technically an extension but let's make that mandatory
};

GraphicsPipelineProperties default_graphics_pipeline_properties(void* pNext = nullptr);

VkPipeline
create_graphics_pipeline(VkDevice device,
                         nonstd::span<const VkPipelineShaderStageCreateInfo>
                                                            shader_stages,
                         const GraphicsPipelineProperties&  properties,
                         nonstd::span<const VkDynamicState> dynamic_states = nonstd::span<const VkDynamicState>());

VkRenderingAttachmentInfo default_rendering_attachment_info(VkImageView image_view, VkImageLayout layout);

VkRenderingInfo default_rendering_info(VkRect2D render_rect, const VkRenderingAttachmentInfo* color_attachment,
                                       const VkRenderingAttachmentInfo* depth_attachment = nullptr);
} // namespace Reaper
