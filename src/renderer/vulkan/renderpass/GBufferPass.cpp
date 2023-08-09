////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GBufferPass.h"

#include "MeshletCulling.h"
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

#include "common/Log.h"
#include "common/ReaperRoot.h"

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
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.gbuffer_write_opaque_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.gbuffer_write_opaque_fs)};

        std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_state = {
            default_pipeline_color_blend_attachment_state(), default_pipeline_color_blend_attachment_state()};

        std::vector<VkFormat> color_formats = {PixelFormatToVulkan(GBufferRT0Format),
                                               PixelFormatToVulkan(GBufferRT1Format)};
        const VkFormat        depth_format = PixelFormatToVulkan(MainPassDepthFormat);

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
        pipeline_properties.input_assembly.primitiveRestartEnable = VK_TRUE;
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

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), VK_FLAGS_NONE);
    bindingFlags[6] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
        create_descriptor_set_layout(backend.device, bindings, bindingFlags),
    };

    resources.pipe.desc_set_layout = descriptorSetLayouts[0];

    resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, descriptorSetLayouts);

    resources.pipe.pipeline = create_gbuffer_pipeline(root, backend, resources.pipe.pipelineLayout, shader_modules);

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

    vmaDestroyBuffer(backend.vma_instance, resources.instancesConstantBuffer.handle,
                     resources.instancesConstantBuffer.allocation);
}

void update_gbuffer_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const GBufferPassResources& resources,
                                         const MeshletCullingResources meshlet_culling_resources,
                                         const SamplerResources&       sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache)
{
    write_helper.append(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.instancesConstantBuffer.handle);
    write_helper.append(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        meshlet_culling_resources.visible_meshlet_buffer.handle);
    write_helper.append(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferPosition.handle);
    write_helper.append(resources.descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferNormal.handle);
    write_helper.append(resources.descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferUV.handle);
    write_helper.append(resources.descriptor_set, 5, sampler_resources.diffuseMapSampler);

    if (!material_resources.texture_handles.empty())
    {
        nonstd::span<VkDescriptorImageInfo> albedo_image_infos =
            write_helper.new_image_infos(material_resources.texture_handles.size());

        for (u32 index = 0; index < albedo_image_infos.size(); index += 1)
        {
            const auto& albedo_map = material_resources.textures[index];
            albedo_image_infos[index] =
                create_descriptor_image_info(albedo_map.default_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

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

    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.instancesConstantBuffer,
                       prepared.forward_instances.data(),
                       prepared.forward_instances.size() * sizeof(ForwardInstanceParams));
}

void record_gbuffer_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                        const GBufferPassResources&    pass_resources,
                                        const MeshletCullingResources& meshlet_culling_resources,
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

    VkRenderingAttachmentInfo rt1_attachment =
        default_rendering_attachment_info(gbuffer_rt1.view_handle, gbuffer_rt1.image_layout);

    std::vector<VkRenderingAttachmentInfo> color_attachments = {rt0_attachment, rt1_attachment};

    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_buffer.view_handle, depth_buffer.image_layout);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, color_attachments, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    const MeshletDrawParams draw_params = get_meshlet_draw_params(prepared.main_culling_pass_index);

    vkCmdBindIndexBuffer(cmdBuffer.handle, meshlet_culling_resources.visible_index_buffer.handle,
                         draw_params.index_buffer_offset, draw_params.index_type);

    std::vector<VkDescriptorSet> pass_descriptors = {
        pass_resources.descriptor_set,
    };

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipelineLayout, 0,
                            static_cast<u32>(pass_descriptors.size()), pass_descriptors.data(), 0, nullptr);

    vkCmdDrawIndexedIndirectCount(
        cmdBuffer.handle, meshlet_culling_resources.visible_indirect_draw_commands_buffer.handle,
        draw_params.command_buffer_offset, meshlet_culling_resources.counters_buffer.handle,
        draw_params.counter_buffer_offset, draw_params.command_buffer_max_count,
        meshlet_culling_resources.visible_indirect_draw_commands_buffer.properties.element_size_bytes);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
