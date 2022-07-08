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
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/Shader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <array>

#include "renderer/shader/share/shadow_map_pass.hlsl"

namespace Reaper
{
constexpr u32 ShadowInstanceCountMax = 512;
constexpr u32 MaxShadowPassCount = 4;

namespace
{
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

    const char*    entryPoint = "main";
    VkShaderModule shader = vulkan_create_shader_module(backend.device, "build/shader/render_shadow.vert.spv");

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                                   nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shader,
                                                                   entryPoint, nullptr}};

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
    };

    resources.pipe.descSetLayout = create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);
    resources.pipe.pipelineLayout =
        create_pipeline_layout(backend.device, nonstd::span(&resources.pipe.descSetLayout, 1));

    GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
    pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
    pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
    pipeline_properties.depth_stencil.depthCompareOp = ShadowUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
    pipeline_properties.pipeline_layout = resources.pipe.pipelineLayout;
    pipeline_properties.pipeline_rendering.depthAttachmentFormat = PixelFormatToVulkan(ShadowMapFormat);

    resources.pipe.pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

    vkDestroyShaderModule(backend.device, shader, nullptr);

    resources.passConstantBuffer = create_buffer(
        root, backend.device, "Shadow Map Pass Constant buffer",
        DefaultGPUBufferProperties(MaxShadowPassCount, sizeof(ShadowMapPassParams), GPUBufferUsage::UniformBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.instanceConstantBuffer =
        create_buffer(root, backend.device, "Shadow Map Instance Constant buffer",
                      DefaultGPUBufferProperties(ShadowInstanceCountMax, sizeof(ShadowMapInstanceParams),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

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
        if (shadow_pass.instance_count > 0)
        {
            update_shadow_map_pass_descriptor_set(backend, pass_resources, shadow_pass, vertex_position_buffer);
        }
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
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.shadow_instance_params.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, resources.passConstantBuffer,
                       prepared.shadow_pass_params.data(),
                       prepared.shadow_pass_params.size() * sizeof(ShadowMapPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, resources.instanceConstantBuffer,
                       prepared.shadow_instance_params.data(),
                       prepared.shadow_instance_params.size() * sizeof(ShadowMapInstanceParams));
}

void record_shadow_map_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                      ShadowMapResources& resources, const nonstd::span<VkImageView> shadow_map_views,
                                      const CullResources& cull_resources)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipeline);

    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        if (shadow_pass.instance_count == 0)
            continue;

        REAPER_GPU_SCOPE(cmdBuffer, fmt::format("Shadow Pass {}", shadow_pass.pass_index).c_str());

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

        const VkViewport viewport = default_vk_viewport(pass_rect);

        vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

        const CullingDrawParams draw_params = get_culling_draw_params(shadow_pass.pass_index);

        vkCmdBindIndexBuffer(cmdBuffer.handle, cull_resources.dynamicIndexBuffer.handle,
                             draw_params.index_buffer_offset, draw_params.index_type);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipelineLayout, 0, 1,
                                &resources.descriptor_sets[shadow_pass.pass_index], 0, nullptr);

        vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, cull_resources.indirectDrawBuffer.handle,
                                      draw_params.command_buffer_offset, cull_resources.countersBuffer.handle,
                                      draw_params.counter_buffer_offset, draw_params.command_buffer_max_count,
                                      cull_resources.indirectDrawBuffer.properties.element_size_bytes);

        vkCmdEndRendering(cmdBuffer.handle);
    }
}
} // namespace Reaper
