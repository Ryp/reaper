////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ForwardPass.h"

#include "Culling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/renderpass/ForwardPassConstants.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/shader/share/forward.hlsl"

namespace Reaper
{
constexpr u32 DiffuseMapMaxCount = 8;
constexpr u32 DrawInstanceCountMax = 512;

namespace
{
    VkDescriptorSetLayout create_descriptor_set_layout_0(VulkanBackend& backend)
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ShadowMapMaxCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags = {
            VK_FLAGS_NONE, VK_FLAGS_NONE, VK_FLAGS_NONE, VK_FLAGS_NONE,
            VK_FLAGS_NONE, VK_FLAGS_NONE, VK_FLAGS_NONE, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

        Assert(bindingFlags.size() == descriptorSetLayoutBinding.size());

        const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags =
            descriptor_set_layout_binding_flags_create_info(bindingFlags);

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo =
            descriptor_set_layout_create_info(descriptorSetLayoutBinding);
        descriptorSetLayoutInfo.pNext = &descriptorSetLayoutBindingFlags;

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &layout) == VK_SUCCESS);

        return layout;
    }

    void update_descriptor_set_0(VulkanBackend& backend, const ForwardPassResources& resources,
                                 const SamplerResources& sampler_resources, const MeshCache& mesh_cache,
                                 const LightingPassResources&    lighting_resources,
                                 const nonstd::span<VkImageView> shadow_map_views)
    {
        const VkDescriptorBufferInfo passParams = default_descriptor_buffer_info(resources.passConstantBuffer);
        const VkDescriptorBufferInfo instanceParams = default_descriptor_buffer_info(resources.instancesConstantBuffer);

        const VkDescriptorBufferInfo vertexPositionParams =
            default_descriptor_buffer_info(mesh_cache.vertexBufferPosition);
        const VkDescriptorBufferInfo vertexNormalParams = default_descriptor_buffer_info(mesh_cache.vertexBufferNormal);
        const VkDescriptorBufferInfo vertexUVParams = default_descriptor_buffer_info(mesh_cache.vertexBufferUV);
        const VkDescriptorBufferInfo pointLightParams =
            default_descriptor_buffer_info(lighting_resources.pointLightBuffer);

        const VkDescriptorImageInfo shadowMapSampler = {sampler_resources.shadowMapSampler, VK_NULL_HANDLE,
                                                        VK_IMAGE_LAYOUT_UNDEFINED};

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &passParams),
            create_buffer_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &instanceParams),
            create_buffer_descriptor_write(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &vertexPositionParams),
            create_buffer_descriptor_write(resources.descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &vertexNormalParams),
            create_buffer_descriptor_write(resources.descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &vertexUVParams),
            create_buffer_descriptor_write(resources.descriptor_set, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &pointLightParams),
            create_image_descriptor_write(resources.descriptor_set, 6, VK_DESCRIPTOR_TYPE_SAMPLER, &shadowMapSampler)};

        std::vector<VkDescriptorImageInfo> drawDescShadowMaps;

        for (auto shadow_map_texture_view : shadow_map_views)
        {
            drawDescShadowMaps.push_back(
                {VK_NULL_HANDLE, shadow_map_texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        const VkWriteDescriptorSet shadowMapBindlessImageWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            resources.descriptor_set,
            7,
            0,
            static_cast<u32>(drawDescShadowMaps.size()),
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            drawDescShadowMaps.data(),
            nullptr,
            nullptr,
        };

        // Only stage descriptor write if we actually have shadow maps to bind
        if (!drawDescShadowMaps.empty())
        {
            writes.push_back(shadowMapBindlessImageWrite);
        }

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    VkDescriptorSetLayout create_descriptor_set_layout_1(VulkanBackend& backend)
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags = {VK_FLAGS_NONE, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

        Assert(bindingFlags.size() == descriptorSetLayoutBinding.size());

        const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags =
            descriptor_set_layout_binding_flags_create_info(bindingFlags);

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo =
            descriptor_set_layout_create_info(descriptorSetLayoutBinding);
        descriptorSetLayoutInfo.pNext = &descriptorSetLayoutBindingFlags;

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &layout) == VK_SUCCESS);

        return layout;
    }

    void update_descriptor_set_1(VulkanBackend& backend, const ForwardPassResources& resources,
                                 const SamplerResources& sampler_resources, const MaterialResources& material_resources)
    {
        if (material_resources.texture_handles.empty())
            return;

        std::vector<VkDescriptorImageInfo> descriptor_maps;

        for (auto handle : material_resources.texture_handles)
        {
            VkImageView image_view = material_resources.textures[handle].default_view;
            descriptor_maps.push_back({VK_NULL_HANDLE, image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        Assert(descriptor_maps.size() <= DiffuseMapMaxCount);

        const VkWriteDescriptorSet diffuseMapBindlessImageWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            resources.material_descriptor_set,
            1,
            0,
            static_cast<u32>(descriptor_maps.size()),
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            descriptor_maps.data(),
            nullptr,
            nullptr,
        };

        const VkDescriptorImageInfo diffuseMapSampler = {sampler_resources.diffuseMapSampler, VK_NULL_HANDLE,
                                                         VK_IMAGE_LAYOUT_UNDEFINED};

        std::vector<VkWriteDescriptorSet> writes = {create_image_descriptor_write(resources.material_descriptor_set, 0,
                                                                                  VK_DESCRIPTOR_TYPE_SAMPLER,
                                                                                  &diffuseMapSampler),
                                                    diffuseMapBindlessImageWrite};

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    VkPipeline create_forward_pipeline(ReaperRoot& root, VulkanBackend& backend, VkPipelineLayout pipeline_layout,
                                       const ShaderModules& shader_modules)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
             shader_modules.forward_vs, default_entry_point(), nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             shader_modules.forward_fs, default_entry_point(), nullptr}};

        const VkPipelineColorBlendAttachmentState blend_attachment_state =
            default_pipeline_color_blend_attachment_state();

        const VkFormat color_format = PixelFormatToVulkan(ForwardHDRColorFormat);
        const VkFormat depth_format = PixelFormatToVulkan(ForwardDepthFormat);

        VkPipelineCreationFeedback              feedback = {};
        std::vector<VkPipelineCreationFeedback> feedback_stages(shader_stages.size());
        VkPipelineCreationFeedbackCreateInfo    feedback_info = {
            VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,
            nullptr,
            &feedback,
            static_cast<u32>(feedback_stages.size()),
            feedback_stages.data(),
        };

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties(&feedback_info);
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthCompareOp =
            ForwardUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        pipeline_properties.blend_state.attachmentCount = 1;
        pipeline_properties.blend_state.pAttachments = &blend_attachment_state;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = 1;
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = &color_format;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = depth_format;

        VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

        Assert(backend.physicalDeviceInfo.graphicsQueueFamilyIndex
               == backend.physicalDeviceInfo.presentQueueFamilyIndex);
        log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

        log_debug(root, "- total time = {}ms, vs = {}ms, fs = {}ms", feedback.duration / 1000,
                  feedback_stages[0].duration / 1000, feedback_stages[1].duration / 1000);

        return pipeline;
    }
} // namespace

ForwardPassResources create_forward_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                   const ShaderModules& shader_modules)
{
    ForwardPassResources resources = {};

    resources.pipe.descSetLayout = create_descriptor_set_layout_0(backend);
    resources.pipe.descSetLayout2 = create_descriptor_set_layout_1(backend);

    std::vector<VkDescriptorSetLayout> passDescriptorSetLayouts = {
        resources.pipe.descSetLayout,
        resources.pipe.descSetLayout2,
    };

    resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, passDescriptorSetLayouts);

    resources.pipe.pipeline = create_forward_pipeline(root, backend, resources.pipe.pipelineLayout, shader_modules);

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "Forward Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(ForwardPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.instancesConstantBuffer = create_buffer(
        root, backend.device, "Forward Instance buffer",
        DefaultGPUBufferProperties(DrawInstanceCountMax, sizeof(ForwardInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    std::vector<VkDescriptorSetLayout> dset_layouts = {resources.pipe.descSetLayout, resources.pipe.descSetLayout2};
    std::vector<VkDescriptorSet>       dsets(dset_layouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

    resources.descriptor_set = dsets[0];
    resources.material_descriptor_set = dsets[1];

    return resources;
}

void destroy_forward_pass_resources(VulkanBackend& backend, ForwardPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.descSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.descSetLayout2, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.handle,
                     resources.passConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.instancesConstantBuffer.handle,
                     resources.instancesConstantBuffer.allocation);
}

void update_forward_pass_descriptor_sets(VulkanBackend& backend, const ForwardPassResources& resources,
                                         const SamplerResources&  sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                         const LightingPassResources&    lighting_resources,
                                         const nonstd::span<VkImageView> shadow_map_views)
{
    update_descriptor_set_0(backend, resources, sampler_resources, mesh_cache, lighting_resources, shadow_map_views);
    update_descriptor_set_1(backend, resources, sampler_resources, material_resources);
}

void upload_forward_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                         ForwardPassResources& pass_resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.forward_instances.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.passConstantBuffer,
                       &prepared.forward_pass_constants, sizeof(ForwardPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.instancesConstantBuffer,
                       prepared.forward_instances.data(),
                       prepared.forward_instances.size() * sizeof(ForwardInstanceParams));
}

void record_forward_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                        const ForwardPassResources& pass_resources, const CullResources& cull_resources,
                                        VkExtent2D backbufferExtent, VkImageView hdrBufferView,
                                        VkImageView depthBufferView)
{
    if (prepared.forward_instances.empty())
        return;

    const VkRect2D   pass_rect = default_vk_rect(backbufferExtent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo color_attachment =
        default_rendering_attachment_info(hdrBufferView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.clearValue = VkClearColor({0.1f, 0.1f, 0.1f, 0.0f});

    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depthBufferView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.clearValue = VkClearDepthStencil(ForwardUseReverseZ ? 0.f : 1.f, 0);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, &color_attachment, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    const CullingDrawParams draw_params = get_culling_draw_params(prepared.forward_culling_pass_index);

    vkCmdBindIndexBuffer(cmdBuffer.handle, cull_resources.dynamicIndexBuffer.handle, draw_params.index_buffer_offset,
                         draw_params.index_type);

    std::array<VkDescriptorSet, 2> pass_descriptors = {
        pass_resources.descriptor_set,
        pass_resources.material_descriptor_set,
    };

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipelineLayout, 0,
                            static_cast<u32>(pass_descriptors.size()), pass_descriptors.data(), 0, nullptr);

    vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, cull_resources.indirectDrawBuffer.handle,
                                  draw_params.command_buffer_offset, cull_resources.countersBuffer.handle,
                                  draw_params.counter_buffer_offset, draw_params.command_buffer_max_count,
                                  cull_resources.indirectDrawBuffer.properties.element_size_bytes);
    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
