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
VkDescriptorSetLayoutBindingFlagsCreateInfo
descriptor_set_layout_binding_flags_create_info(nonstd::span<const VkDescriptorBindingFlags> binding_flags);

VkDescriptorSetLayoutCreateInfo
descriptor_set_layout_create_info(nonstd::span<const VkDescriptorSetLayoutBinding> layout_bindings);

VkDescriptorSetLayout create_descriptor_set_layout(VkDevice                                         device,
                                                   nonstd::span<const VkDescriptorSetLayoutBinding> layout_bindings);

VkPipelineLayout create_pipeline_layout(
    VkDevice device, nonstd::span<const VkDescriptorSetLayout> descriptor_set_layouts,
    nonstd::span<const VkPushConstantRange> push_constant_ranges = nonstd::span<const VkPushConstantRange>());

VkPipeline create_compute_pipeline(VkDevice device, VkPipelineLayout pipeline_layout, VkShaderModule compute_shader,
                                   VkSpecializationInfo* specialization_info = nullptr);
} // namespace Reaper
