////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Pipeline.h"

#include <core/Assert.h>

namespace Reaper
{
VkDescriptorSetLayoutBindingFlagsCreateInfo
descriptor_set_layout_binding_flags_create_info(nonstd::span<const VkDescriptorBindingFlags> binding_flags)
{
    return VkDescriptorSetLayoutBindingFlagsCreateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr,
        static_cast<u32>(binding_flags.size()), binding_flags.data()};
}

VkDescriptorSetLayoutCreateInfo
descriptor_set_layout_create_info(nonstd::span<const VkDescriptorSetLayoutBinding> layout_bindings)
{
    return VkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
                                           static_cast<u32>(layout_bindings.size()), layout_bindings.data()};
}

VkDescriptorSetLayout
create_descriptor_set_layout(VkDevice device, nonstd::span<const VkDescriptorSetLayoutBinding> layout_bindings)
{
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = descriptor_set_layout_create_info(layout_bindings);

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout) == VK_SUCCESS);

    return descriptorSetLayout;
}

VkPipelineLayout create_pipeline_layout(VkDevice device,
                                        nonstd::span<const VkDescriptorSetLayout>
                                            descriptor_set_layouts,
                                        nonstd::span<const VkPushConstantRange>
                                            push_constant_ranges)
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                     nullptr,
                                                     VK_FLAGS_NONE,
                                                     static_cast<u32>(descriptor_set_layouts.size()),
                                                     descriptor_set_layouts.data(),
                                                     static_cast<u32>(push_constant_ranges.size()),
                                                     push_constant_ranges.data()};

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Assert(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

    return pipelineLayout;
}

VkPipeline create_compute_pipeline(VkDevice              device,
                                   VkPipelineLayout      pipeline_layout,
                                   VkShaderModule        compute_shader,
                                   VkSpecializationInfo* specialization_info)
{
    static const char* DefaultShaderEntryPoint = "main";

    VkPipelineShaderStageCreateInfo shaderStage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                   nullptr,
                                                   0,
                                                   VK_SHADER_STAGE_COMPUTE_BIT,
                                                   compute_shader,
                                                   DefaultShaderEntryPoint,
                                                   specialization_info};

    VkComputePipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                      nullptr,
                                                      0,
                                                      shaderStage,
                                                      pipeline_layout,
                                                      VK_NULL_HANDLE, // do not care about pipeline derivatives
                                                      0};

    VkPipeline      pipeline = VK_NULL_HANDLE;
    VkPipelineCache cache = VK_NULL_HANDLE;

    Assert(vkCreateComputePipelines(device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline) == VK_SUCCESS);

    return pipeline;
}
} // namespace Reaper
