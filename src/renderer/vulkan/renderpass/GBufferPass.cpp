////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GBufferPass.h"

#include "Culling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/renderpass/Constants.h"
#include "renderer/vulkan/renderpass/GBufferPassConstants.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

// #include "renderer/shader/share/gbuffer.hlsl"

#include <vector>

namespace Reaper
{
constexpr u32 GBufferInstanceCountMax = 512;

namespace
{
    VkPipeline create_gbuffer_pipeline(ReaperRoot& root, VulkanBackend& backend, VkPipelineLayout pipeline_layout,
                                       const ShaderModules& shader_modules)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
             shader_modules.gbuffer_write_opaque_vs, default_entry_point(), nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             shader_modules.gbuffer_write_opaque_fs, default_entry_point(), nullptr}};

        std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_state = {
            default_pipeline_color_blend_attachment_state(), default_pipeline_color_blend_attachment_state()};

        std::vector<VkFormat> color_formats = {PixelFormatToVulkan(GBufferRT0Format),
                                               PixelFormatToVulkan(GBufferRT1Format)};
        const VkFormat        depth_format = PixelFormatToVulkan(GBufferDepthFormat);

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
            MainPassUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        pipeline_properties.blend_state.attachmentCount = blend_attachment_state.size();
        pipeline_properties.blend_state.pAttachments = blend_attachment_state.data();
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = color_formats.size();
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = color_formats.data();
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

GBufferPassResources create_gbuffer_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                   const ShaderModules& shader_modules)
{
    GBufferPassResources resources = {};

    std::vector<VkDescriptorSetLayoutBinding> bindings0 = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    std::vector<VkDescriptorBindingFlags> bindingFlags0(bindings0.size(), VK_FLAGS_NONE);
    bindingFlags0[6] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
        create_descriptor_set_layout(backend.device, bindings0, bindingFlags0),
    };

    resources.pipe.desc_set_layout = descriptorSetLayouts[0];

    resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, descriptorSetLayouts);

    resources.pipe.pipeline = create_gbuffer_pipeline(root, backend, resources.pipe.pipelineLayout, shader_modules);

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "GBuffer Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(ForwardPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.instancesConstantBuffer =
        create_buffer(root, backend.device, "GBuffer Instance buffer",
                      DefaultGPUBufferProperties(GBufferInstanceCountMax, sizeof(ForwardInstanceParams),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    std::vector<VkDescriptorSet> dsets(descriptorSetLayouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, descriptorSetLayouts, dsets);

    resources.descriptor_set = dsets[0];

    return resources;
}

void destroy_gbuffer_pass_resources(VulkanBackend& backend, GBufferPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.desc_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.desc_set_layout_material, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.handle,
                     resources.passConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.instancesConstantBuffer.handle,
                     resources.instancesConstantBuffer.allocation);
}

void update_gbuffer_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const GBufferPassResources& resources,
                                         const SamplerResources&  sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache)
{
    append_write(write_helper, resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 resources.passConstantBuffer.handle);
    append_write(write_helper, resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 resources.instancesConstantBuffer.handle);
    append_write(write_helper, resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 mesh_cache.vertexBufferPosition.handle);
    append_write(write_helper, resources.descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 mesh_cache.vertexBufferNormal.handle);
    append_write(write_helper, resources.descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 mesh_cache.vertexBufferUV.handle);
    append_write(write_helper, resources.descriptor_set, 5, sampler_resources.diffuseMapSampler);

    if (!material_resources.texture_handles.empty())
    {
        const VkDescriptorImageInfo* image_info_ptr = write_helper.images.data() + write_helper.images.size();

        for (auto handle : material_resources.texture_handles)
        {
            write_helper.images.push_back({VK_NULL_HANDLE, material_resources.textures[handle].default_view,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        nonstd::span<const VkDescriptorImageInfo> albedo_image_infos(image_info_ptr,
                                                                     material_resources.texture_handles.size());

        Assert(albedo_image_infos.size() <= DiffuseMapMaxCount);

        write_helper.writes.push_back(create_image_descriptor_write(
            resources.descriptor_set, 6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, albedo_image_infos));
    }
}

void upload_gbuffer_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                         GBufferPassResources& pass_resources)
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

void record_gbuffer_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                        const GBufferPassResources& pass_resources, const CullResources& cull_resources,
                                        const FrameGraphTexture& gbuffer_rt0, const FrameGraphTexture& gbuffer_rt1,
                                        const FrameGraphTexture& depth_buffer)
{
    if (prepared.forward_instances.empty())
        return;

    const VkExtent2D extent = {depth_buffer.properties.width, depth_buffer.properties.height};
    const VkRect2D   pass_rect = default_vk_rect(extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo rt0_attachment =
        default_rendering_attachment_info(gbuffer_rt0.view_handle, gbuffer_rt0.image_layout);
    rt0_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    VkRenderingAttachmentInfo rt1_attachment =
        default_rendering_attachment_info(gbuffer_rt1.view_handle, gbuffer_rt1.image_layout);
    rt1_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    std::vector<VkRenderingAttachmentInfo> color_attachments = {rt0_attachment, rt1_attachment};

    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_buffer.view_handle, depth_buffer.image_layout);
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.clearValue = VkClearDepthStencil(MainPassUseReverseZ ? 0.f : 1.f, 0);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, color_attachments, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    const CullingDrawParams draw_params = get_culling_draw_params(prepared.forward_culling_pass_index);

    vkCmdBindIndexBuffer(cmdBuffer.handle, cull_resources.dynamicIndexBuffer.handle, draw_params.index_buffer_offset,
                         draw_params.index_type);

    std::vector<VkDescriptorSet> pass_descriptors = {
        pass_resources.descriptor_set,
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
