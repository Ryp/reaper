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

    const VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = static_cast<u32>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
    };

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
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = static_cast<u32>(binding_flags.size()),
        .pBindingFlags = binding_flags.data()};
}

VkDescriptorSetLayoutCreateInfo
descriptor_set_layout_create_info(nonstd::span<const VkDescriptorSetLayoutBinding> layout_bindings)
{
    return VkDescriptorSetLayoutCreateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .pNext = nullptr,
                                           .flags = VK_FLAGS_NONE,
                                           .bindingCount = static_cast<u32>(layout_bindings.size()),
                                           .pBindings = layout_bindings.data()};
}

VkDescriptorSetLayout create_descriptor_set_layout(VkDevice device,
                                                   nonstd::span<const VkDescriptorSetLayoutBinding>
                                                       layout_bindings,
                                                   nonstd::span<const VkDescriptorBindingFlags>
                                                       binding_flags)
{
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = descriptor_set_layout_create_info(layout_bindings);
    VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags;

    if (!binding_flags.empty())
    {
        Assert(binding_flags.size() == layout_bindings.size());

        descriptorSetLayoutBindingFlags = descriptor_set_layout_binding_flags_create_info(binding_flags);

        descriptorSetLayoutInfo.pNext = &descriptorSetLayoutBindingFlags;
    }

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
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = VK_FLAGS_NONE,
                                                     .setLayoutCount = static_cast<u32>(descriptor_set_layouts.size()),
                                                     .pSetLayouts = descriptor_set_layouts.data(),
                                                     .pushConstantRangeCount =
                                                         static_cast<u32>(push_constant_ranges.size()),
                                                     .pPushConstantRanges = push_constant_ranges.data()};

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Assert(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

    return pipelineLayout;
}

const char* default_entry_point()
{
    return "main";
}

VkPipeline create_compute_pipeline(VkDevice device, VkPipelineLayout pipeline_layout,
                                   const VkPipelineShaderStageCreateInfo& shader_stage_create_info)
{
    VkComputePipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .stage = shader_stage_create_info,
        .layout = pipeline_layout,
        .basePipelineHandle = VK_NULL_HANDLE, // do not care about pipeline derivatives
        .basePipelineIndex = 0,
    };

    VkPipeline      pipeline = VK_NULL_HANDLE;
    VkPipelineCache cache = VK_NULL_HANDLE;

    Assert(vkCreateComputePipelines(device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline) == VK_SUCCESS);

    return pipeline;
}

VkPipeline create_compute_pipeline(VkDevice              device,
                                   VkPipelineLayout      pipeline_layout,
                                   VkShaderModule        compute_shader,
                                   VkSpecializationInfo* specialization_info)
{
    VkPipelineShaderStageCreateInfo shader_stage =
        default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, compute_shader, specialization_info);

    return create_compute_pipeline(device, pipeline_layout, shader_stage);
}

VkPipelineShaderStageCreateInfo
default_pipeline_shader_stage_create_info(VkShaderStageFlagBits stage_bit, VkShaderModule shader_module,
                                          const VkSpecializationInfo*      specialization_info,
                                          VkPipelineShaderStageCreateFlags flags)
{
    return VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .stage = stage_bit,
        .module = shader_module,
        .pName = default_entry_point(),
        .pSpecializationInfo = specialization_info,
    };
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
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = VK_FLAGS_NONE,
        .colorAttachmentCount = 0,
        .pColorAttachmentFormats = nullptr,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };
}

VkPipelineVertexInputStateCreateInfo default_pipeline_vertex_input_state_create_info()
{
    return VkPipelineVertexInputStateCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = VK_FLAGS_NONE,
                                                .vertexBindingDescriptionCount = 0,
                                                .pVertexBindingDescriptions = nullptr,
                                                .vertexAttributeDescriptionCount = 0,
                                                .pVertexAttributeDescriptions = nullptr};
}

VkPipelineInputAssemblyStateCreateInfo default_pipeline_input_assembly_state_create_info()
{
    return VkPipelineInputAssemblyStateCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                  .pNext = nullptr,
                                                  .flags = VK_FLAGS_NONE,
                                                  .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                  .primitiveRestartEnable = VK_FALSE};
}

VkPipelineViewportStateCreateInfo default_pipeline_viewport_state_create_info()
{
    return VkPipelineViewportStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .viewportCount = 1,
        .pViewports = nullptr, // dynamic viewpor
        .scissorCount = 1,
        .pScissors = nullptr // dynamic scissors
    };
}

VkPipelineRasterizationStateCreateInfo default_pipeline_rasterization_state_create_info()
{
    return VkPipelineRasterizationStateCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                  .pNext = nullptr,
                                                  .flags = VK_FLAGS_NONE,
                                                  .depthClampEnable = VK_FALSE,
                                                  .rasterizerDiscardEnable = VK_FALSE,
                                                  .polygonMode = VK_POLYGON_MODE_FILL,
                                                  .cullMode = VK_CULL_MODE_BACK_BIT,
                                                  .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                  .depthBiasEnable = VK_FALSE,
                                                  .depthBiasConstantFactor = 0.f,
                                                  .depthBiasClamp = 0.f,
                                                  .depthBiasSlopeFactor = 0.f,
                                                  .lineWidth = 1.f};
}

VkPipelineMultisampleStateCreateInfo default_pipeline_multisample_state_create_info()
{
    return VkPipelineMultisampleStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };
}

VkPipelineDepthStencilStateCreateInfo default_pipeline_depth_stencil_state_create_info()
{
    return VkPipelineDepthStencilStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = VkStencilOpState{},
        .back = VkStencilOpState{},
        .minDepthBounds = 0.f,
        .maxDepthBounds = 1.f,
    };
}

VkPipelineColorBlendStateCreateInfo default_pipeline_color_blend_state_create_info()
{
    return VkPipelineColorBlendStateCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = VK_FLAGS_NONE,
                                               .logicOpEnable = VK_FALSE,
                                               .logicOp = VK_LOGIC_OP_COPY,
                                               .attachmentCount = 0,
                                               .pAttachments = nullptr,
                                               .blendConstants = {0.f, 0.f, 0.f, 0.f}};
}

GraphicsPipelineProperties default_graphics_pipeline_properties(void* pNext)
{
    VkPipelineRenderingCreateInfo pipeline_rendering = default_pipeline_rendering_create_info();
    pipeline_rendering.pNext = pNext;

    return GraphicsPipelineProperties{
        .vertex_input = default_pipeline_vertex_input_state_create_info(),
        .input_assembly = default_pipeline_input_assembly_state_create_info(),
        .viewport = default_pipeline_viewport_state_create_info(),
        .raster = default_pipeline_rasterization_state_create_info(),
        .multisample = default_pipeline_multisample_state_create_info(),
        .depth_stencil = default_pipeline_depth_stencil_state_create_info(),
        .blend_state = default_pipeline_color_blend_state_create_info(),
        .pipeline_layout = VK_NULL_HANDLE,
        .pipeline_rendering = pipeline_rendering,
    };
}

VkPipelineDynamicStateCreateInfo
create_pipeline_dynamic_state_create_info(nonstd::span<const VkDynamicState> dynamic_states)
{
    return VkPipelineDynamicStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };
}

VkPipeline create_graphics_pipeline(VkDevice device,
                                    nonstd::span<const VkPipelineShaderStageCreateInfo>
                                                                      shader_stages,
                                    const GraphicsPipelineProperties& properties,
                                    nonstd::span<const VkDynamicState>
                                        dynamic_states)
{
    const std::vector<VkDynamicState> default_dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic_state =
        create_pipeline_dynamic_state_create_info(dynamic_states.empty() ? default_dynamic_states : dynamic_states);

    VkGraphicsPipelineCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                .pNext = &properties.pipeline_rendering,
                                                .flags = VK_FLAGS_NONE,
                                                .stageCount = static_cast<u32>(shader_stages.size()),
                                                .pStages = shader_stages.data(),
                                                .pVertexInputState = &properties.vertex_input,
                                                .pInputAssemblyState = &properties.input_assembly,
                                                .pTessellationState = nullptr,
                                                .pViewportState = &properties.viewport,
                                                .pRasterizationState = &properties.raster,
                                                .pMultisampleState = &properties.multisample,
                                                .pDepthStencilState = &properties.depth_stencil,
                                                .pColorBlendState = &properties.blend_state,
                                                .pDynamicState = &dynamic_state,
                                                .layout = properties.pipeline_layout,
                                                .renderPass = VK_NULL_HANDLE,
                                                .subpass = 0,
                                                .basePipelineHandle = VK_NULL_HANDLE,
                                                .basePipelineIndex = -1};

    VkPipeline      pipeline = VK_NULL_HANDLE;
    VkPipelineCache cache = VK_NULL_HANDLE;

    Assert(vkCreateGraphicsPipelines(device, cache, 1, &create_info, nullptr, &pipeline) == VK_SUCCESS);

    return pipeline;
} // namespace Reaper

VkRenderingAttachmentInfo default_rendering_attachment_info(VkImageView image_view, VkImageLayout layout)
{
    return VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = image_view,
        .imageLayout = layout,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{},
    };
}

VkRenderingInfo default_rendering_info(VkRect2D render_rect, const VkRenderingAttachmentInfo* color_attachment,
                                       const VkRenderingAttachmentInfo* depth_attachment)
{
    auto color_attachments =
        color_attachment ? nonstd::make_span(color_attachment, 1) : nonstd::span<const VkRenderingAttachmentInfo>();

    return default_rendering_info(render_rect, color_attachments, depth_attachment);
}

VkRenderingInfo default_rendering_info(VkRect2D render_rect,
                                       nonstd::span<const VkRenderingAttachmentInfo>
                                                                        color_attachments,
                                       const VkRenderingAttachmentInfo* depth_attachment)
{
    return VkRenderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .renderArea = render_rect,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<u32>(color_attachments.size()),
        .pColorAttachments = color_attachments.data(),
        .pDepthAttachment = depth_attachment,
        .pStencilAttachment = nullptr,
    };
}
} // namespace Reaper
