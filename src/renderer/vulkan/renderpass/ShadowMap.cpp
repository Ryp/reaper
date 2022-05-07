////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ShadowMap.h"

#include "Culling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/api/Vulkan.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/Profile.h"

#include <array>

#include "renderer/shader/share/shadow_map_pass.hlsl"

namespace Reaper
{
constexpr u32 ShadowInstanceCountMax = 512;
constexpr u32 MaxShadowPassCount = 4;

namespace
{
    ShadowMapPipelineInfo create_shadow_map_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        const char*           fileNameVS = "./build/shader/render_shadow.vert.spv";
        const char*           fileNameFS = "./build/shader/render_shadow.frag.spv";
        const char*           entryPoint = "main";
        VkSpecializationInfo* specialization = nullptr;
        VkShaderModule        blitShaderVS = vulkan_create_shader_module(backend.device, fileNameVS);
        VkShaderModule        blitShaderFS = vulkan_create_shader_module(backend.device, fileNameFS);

        std::vector<VkPipelineShaderStageCreateInfo> blitShaderStages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, blitShaderVS,
             entryPoint, specialization},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             blitShaderFS, entryPoint, specialization}};

        VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 0, nullptr};

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

        VkPipelineViewportStateCreateInfo blitViewportStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            1,
            nullptr, // dynamic viewport
            1,
            nullptr}; // dynamic scissors

        VkPipelineRasterizationStateCreateInfo blitRasterStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_FALSE,
            VK_FALSE,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            VK_FALSE,
            0.0f,
            0.0f,
            0.0f,
            1.0f};

        VkPipelineMultisampleStateCreateInfo blitMSStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            1.0f,
            nullptr,
            VK_FALSE,
            VK_FALSE};

        const VkPipelineDepthStencilStateCreateInfo blitDepthStencilInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_TRUE, // depth test
            VK_TRUE, // depth write
            ShadowUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_FALSE,
            VkStencilOpState{},
            VkStencilOpState{},
            0.f,
            0.f};

        VkPipelineColorBlendAttachmentState blitBlendAttachmentState = {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

        VkPipelineColorBlendStateCreateInfo blitBlendStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &blitBlendAttachmentState,
            {0.0f, 0.0f, 0.0f, 0.0f}};

        std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayoutCB = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutCB)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayoutCB));

        VkPipelineLayoutCreateInfo blitPipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                             nullptr,
                                                             VK_FLAGS_NONE,
                                                             1,
                                                             &descriptorSetLayoutCB,
                                                             0,
                                                             nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &blitPipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkPipelineCache cache = VK_NULL_HANDLE;

        const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo    blitDynamicState = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamicStates.size(),
            dynamicStates.data(),
        };

        const VkFormat shadowMapFormat = PixelFormatToVulkan(ShadowMapFormat);

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0, // viewMask
            0,
            nullptr,
            shadowMapFormat,
            VK_FORMAT_UNDEFINED,
        };

        VkGraphicsPipelineCreateInfo blitPipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                               &pipelineRenderingCreateInfo,
                                                               VK_FLAGS_NONE,
                                                               static_cast<u32>(blitShaderStages.size()),
                                                               blitShaderStages.data(),
                                                               &vertexInputStateInfo,
                                                               &inputAssemblyInfo,
                                                               nullptr,
                                                               &blitViewportStateInfo,
                                                               &blitRasterStateInfo,
                                                               &blitMSStateInfo,
                                                               &blitDepthStencilInfo,
                                                               &blitBlendStateInfo,
                                                               &blitDynamicState,
                                                               pipelineLayout,
                                                               VK_NULL_HANDLE,
                                                               0,
                                                               VK_NULL_HANDLE,
                                                               -1};

        VkPipeline pipeline = VK_NULL_HANDLE;
        Assert(vkCreateGraphicsPipelines(backend.device, cache, 1, &blitPipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

        vkDestroyShaderModule(backend.device, blitShaderVS, nullptr);
        vkDestroyShaderModule(backend.device, blitShaderFS, nullptr);

        return ShadowMapPipelineInfo{pipeline, pipelineLayout, descriptorSetLayoutCB};
    }

    VkDescriptorSet create_shadow_map_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                          VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1, &layout};

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

        return descriptor_set;
    }

    void update_shadow_map_pass_descriptor_set(VulkanBackend& backend, const ShadowMapResources& resources,
                                               const ShadowPassData& shadow_pass, BufferInfo& vertex_position_buffer)
    {
        Assert(shadow_pass.pass_index < resources.descriptor_sets.size());
        VkDescriptorSet descriptor_set = resources.descriptor_sets[shadow_pass.pass_index];

        const VkDescriptorBufferInfo descPassParams =
            get_vk_descriptor_buffer_info(resources.passConstantBuffer, BufferSubresource{shadow_pass.pass_index, 1});
        const VkDescriptorBufferInfo descInstanceParams =
            get_vk_descriptor_buffer_info(resources.instanceConstantBuffer,
                                          BufferSubresource{shadow_pass.instance_offset, shadow_pass.instance_count});
        const VkDescriptorBufferInfo descVertexPosition = default_descriptor_buffer_info(vertex_position_buffer);

        std::array<VkWriteDescriptorSet, 3> shadowMapPassDescriptorSetWrites = {
            create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descPassParams),
            create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descInstanceParams),
            create_buffer_descriptor_write(descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descVertexPosition),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(shadowMapPassDescriptorSetWrites.size()),
                               shadowMapPassDescriptorSetWrites.data(), 0, nullptr);
    }
} // namespace

ShadowMapResources create_shadow_map_resources(ReaperRoot& root, VulkanBackend& backend)
{
    ShadowMapResources resources = {};

    resources.pipe = create_shadow_map_pipeline(root, backend);

    resources.passConstantBuffer = create_buffer(
        root, backend.device, "Shadow Map Pass Constant buffer",
        DefaultGPUBufferProperties(MaxShadowPassCount, sizeof(ShadowMapPassParams), GPUBufferUsage::UniformBuffer),
        backend.vma_instance);

    resources.instanceConstantBuffer =
        create_buffer(root, backend.device, "Shadow Map Instance Constant buffer",
                      DefaultGPUBufferProperties(ShadowInstanceCountMax, sizeof(ShadowMapInstanceParams),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.descriptor_sets.push_back(
        create_shadow_map_pass_descriptor_set(root, backend, resources.pipe.descSetLayout));
    resources.descriptor_sets.push_back(
        create_shadow_map_pass_descriptor_set(root, backend, resources.pipe.descSetLayout));
    resources.descriptor_sets.push_back(
        create_shadow_map_pass_descriptor_set(root, backend, resources.pipe.descSetLayout));

    return resources;
}

void destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.handle,
                     resources.passConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.instanceConstantBuffer.handle,
                     resources.instanceConstantBuffer.allocation);

    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.descSetLayout, nullptr);
}

void update_shadow_map_pass_descriptor_sets(VulkanBackend& backend, const PreparedData& prepared,
                                            ShadowMapResources& pass_resources, BufferInfo& vertex_position_buffer)
{
    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        update_shadow_map_pass_descriptor_set(backend, pass_resources, shadow_pass, vertex_position_buffer);
    }
}

std::vector<GPUTextureProperties> fill_shadow_map_properties(const PreparedData& prepared)
{
    std::vector<GPUTextureProperties> shadow_map_properties;

    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        GPUTextureProperties properties =
            DefaultGPUTextureProperties(shadow_pass.shadow_map_size.x, shadow_pass.shadow_map_size.y, ShadowMapFormat);
        properties.usage_flags =
            GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::InputAttachment | GPUTextureUsage::Sampled;

        shadow_map_properties.emplace_back(properties);
    }

    return shadow_map_properties;
}

void upload_shadow_map_resources(VulkanBackend& backend, const PreparedData& prepared, ShadowMapResources& resources)
{
    upload_buffer_data(backend.device, backend.vma_instance, resources.passConstantBuffer,
                       prepared.shadow_pass_params.data(),
                       prepared.shadow_pass_params.size() * sizeof(ShadowMapPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, resources.instanceConstantBuffer,
                       prepared.shadow_instance_params.data(),
                       prepared.shadow_instance_params.size() * sizeof(ShadowMapInstanceParams));
}

void record_shadow_map_command_buffer(CommandBuffer& cmdBuffer, VulkanBackend& backend, const PreparedData& prepared,
                                      ShadowMapResources& resources, const nonstd::span<VkImageView> shadow_map_views,
                                      const CullResources& cull_resources)
{
    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Shadow Pass", MP_DARKGOLDENROD);

        const VkImageView  shadowMapView = shadow_map_views[shadow_pass.pass_index];
        const VkExtent2D   output_extent = {shadow_pass.shadow_map_size.x, shadow_pass.shadow_map_size.y};
        const VkRect2D     pass_rect = default_vk_rect(output_extent);
        const VkClearValue shadowMapClearValue =
            VkClearDepthStencil(ShadowUseReverseZ ? 0.f : 1.f, 0); // NOTE: handle reverse Z more gracefully

        const VkRenderingAttachmentInfo depth_attachment = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            nullptr,
            shadowMapView,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_RESOLVE_MODE_NONE,
            VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            shadowMapClearValue,
        };

        VkRenderingInfo renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO,
            nullptr,
            VK_FLAGS_NONE,
            pass_rect,
            1, // layerCount
            0, // viewMask
            0,
            nullptr,
            &depth_attachment,
            nullptr,
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderingInfo);

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipeline);

        const VkViewport viewport = default_vk_viewport(pass_rect);

        vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

        vkCmdBindIndexBuffer(cmdBuffer.handle, cull_resources.dynamicIndexBuffer.handle, 0,
                             get_vk_culling_index_type());
        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipelineLayout, 0, 1,
                                &resources.descriptor_sets[shadow_pass.pass_index], 0, nullptr);

        const u32 draw_buffer_offset =
            shadow_pass.culling_pass_index * MaxIndirectDrawCount * sizeof(VkDrawIndexedIndirectCommand);
        const u32 draw_buffer_max_count = MaxIndirectDrawCount;

        if (backend.options.use_compacted_draw)
        {
            const u32 draw_buffer_count_offset = shadow_pass.culling_pass_index * 1 * sizeof(u32);
            vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, cull_resources.compactIndirectDrawBuffer.handle,
                                          draw_buffer_offset, cull_resources.compactIndirectDrawCountBuffer.handle,
                                          draw_buffer_count_offset, draw_buffer_max_count,
                                          cull_resources.compactIndirectDrawBuffer.properties.element_size_bytes);
        }
        else
        {
            const u32 draw_buffer_count_offset = shadow_pass.culling_pass_index * IndirectDrawCountCount * sizeof(u32);
            vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, cull_resources.indirectDrawBuffer.handle,
                                          draw_buffer_offset, cull_resources.indirectDrawCountBuffer.handle,
                                          draw_buffer_count_offset, draw_buffer_max_count,
                                          cull_resources.indirectDrawBuffer.properties.element_size_bytes);
        }

        vkCmdEndRendering(cmdBuffer.handle);
    }
}
} // namespace Reaper
