////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Pipeline.h"

#include <core/Assert.h>
#include <vector>

namespace Reaper
{
void allocate_descriptor_sets(VkDevice device, VkDescriptorPool descriptor_pool,
                              nonstd::span<const VkDescriptorSetLayout> descriptor_set_layouts,
                              nonstd::span<VkDescriptorSet>             output_descriptor_sets)
{
    Assert(descriptor_set_layouts.size() == output_descriptor_sets.size());

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptor_pool,
        static_cast<u32>(descriptor_set_layouts.size()), descriptor_set_layouts.data()};

    Assert(vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, output_descriptor_sets.data()) == VK_SUCCESS);
}

void allocate_descriptor_sets(VkDevice device, VkDescriptorPool descriptor_pool, VkDescriptorSetLayout set_layout,
                              nonstd::span<VkDescriptorSet> output_descriptor_sets)
{
    std::vector<VkDescriptorSetLayout> layouts(output_descriptor_sets.size(), set_layout);

    allocate_descriptor_sets(device, descriptor_pool, layouts, output_descriptor_sets);
}

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

VkDescriptorSetLayout create_descriptor_set_layout(VkDevice                                         device,
                                                   nonstd::span<const VkDescriptorSetLayoutBinding> layout_bindings)
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

const char* default_entry_point()
{
    return "main";
}

VkPipeline create_compute_pipeline(VkDevice              device,
                                   VkPipelineLayout      pipeline_layout,
                                   VkShaderModule        compute_shader,
                                   VkSpecializationInfo* specialization_info)
{

    VkPipelineShaderStageCreateInfo shaderStage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                   nullptr,
                                                   0,
                                                   VK_SHADER_STAGE_COMPUTE_BIT,
                                                   compute_shader,
                                                   default_entry_point(),
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

VkPipelineColorBlendAttachmentState default_pipeline_color_blend_attachment_state()
{
    return VkPipelineColorBlendAttachmentState{VK_FALSE,
                                               VK_BLEND_FACTOR_ONE,
                                               VK_BLEND_FACTOR_ZERO,
                                               VK_BLEND_OP_ADD,
                                               VK_BLEND_FACTOR_ONE,
                                               VK_BLEND_FACTOR_ZERO,
                                               VK_BLEND_OP_ADD,
                                               VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                                   | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
}

VkPipelineRenderingCreateInfo default_pipeline_rendering_create_info()
{
    return VkPipelineRenderingCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        nullptr,
        0, // viewMask
        0,
        nullptr,
        VK_FORMAT_UNDEFINED,
        VK_FORMAT_UNDEFINED,
    };
}

GraphicsPipelineProperties default_graphics_pipeline_properties(void* pNext)
{
    VkPipelineRenderingCreateInfo pipeline_rendering = default_pipeline_rendering_create_info();
    pipeline_rendering.pNext = pNext;

    return GraphicsPipelineProperties{
        VkPipelineVertexInputStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr,
                                             VK_FLAGS_NONE, 0, nullptr, 0, nullptr},
        VkPipelineInputAssemblyStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr,
                                               VK_FLAGS_NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE},
        VkPipelineRasterizationStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr,
                                               VK_FLAGS_NONE, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL,
                                               VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0.0f,
                                               0.0f, 0.0f, 1.0f},
        VkPipelineMultisampleStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr,
                                             VK_FLAGS_NONE, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 1.0f, nullptr, VK_FALSE,
                                             VK_FALSE},
        VkPipelineDepthStencilStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0,
                                              VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE,
                                              VkStencilOpState{}, VkStencilOpState{}, 0.f, 0.f},
        VkPipelineColorBlendStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                            nullptr,
                                            VK_FLAGS_NONE,
                                            VK_FALSE,
                                            VK_LOGIC_OP_COPY,
                                            0,
                                            nullptr,
                                            {0.0f, 0.0f, 0.0f, 0.0f}},
        VK_NULL_HANDLE, // Pipeline layout
        pipeline_rendering,
    };
}

VkPipeline create_graphics_pipeline(VkDevice device,
                                    nonstd::span<const VkPipelineShaderStageCreateInfo>
                                                                      shader_stages,
                                    const GraphicsPipelineProperties& properties)
{
    VkPipelineViewportStateCreateInfo viewport_state = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                        nullptr,
                                                        VK_FLAGS_NONE,
                                                        1,
                                                        nullptr, // dynamic viewport
                                                        1,
                                                        nullptr}; // dynamic scissors

    const std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo    dynamic_state = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0,
        static_cast<u32>(dynamic_states.size()),
        dynamic_states.data(),
    };

    VkGraphicsPipelineCreateInfo create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                &properties.pipeline_rendering,
                                                VK_FLAGS_NONE,
                                                static_cast<u32>(shader_stages.size()),
                                                shader_stages.data(),
                                                &properties.vertex_input,
                                                &properties.input_assembly,
                                                nullptr,
                                                &viewport_state,
                                                &properties.raster,
                                                &properties.multisample,
                                                &properties.depth_stencil,
                                                &properties.blend_state,
                                                &dynamic_state,
                                                properties.pipeline_layout,
                                                VK_NULL_HANDLE,
                                                0,
                                                VK_NULL_HANDLE,
                                                -1};

    VkPipeline      pipeline = VK_NULL_HANDLE;
    VkPipelineCache cache = VK_NULL_HANDLE;

    Assert(vkCreateGraphicsPipelines(device, cache, 1, &create_info, nullptr, &pipeline) == VK_SUCCESS);

    return pipeline;
}

VkRenderingAttachmentInfo default_rendering_attachment_info(VkImageView image_view, VkImageLayout layout)
{
    return VkRenderingAttachmentInfo{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        image_view,
        layout,
        VK_RESOLVE_MODE_NONE,
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        VkClearValue{},
    };
}

VkRenderingInfo default_rendering_info(VkRect2D render_rect, const VkRenderingAttachmentInfo* color_attachment,
                                       const VkRenderingAttachmentInfo* depth_attachment)
{
    return VkRenderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        VK_FLAGS_NONE,
        render_rect,
        1, // layerCount
        0, // viewMask
        (color_attachment != nullptr) ? 1u : 0u,
        color_attachment,
        depth_attachment,
        nullptr,
    };
}
} // namespace Reaper
