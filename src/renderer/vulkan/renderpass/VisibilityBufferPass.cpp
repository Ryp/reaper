////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VisibilityBufferPass.h"

#include "MeshletCulling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
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
#include "renderer/vulkan/renderpass/VisibilityBufferConstants.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/shader/vis_buffer/fill_gbuffer.share.hlsl"

#include <vector>

namespace Reaper
{
constexpr u32 VisibilityBufferInstanceCountMax = 512;

namespace
{
    VkPipeline create_vis_buffer_pipeline(ReaperRoot& root, VulkanBackend& backend, VkPipelineLayout pipeline_layout,
                                          const ShaderModules& shader_modules)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shader_modules.vis_buffer_raster_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.vis_buffer_raster_fs),
        };

        std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_state = {
            default_pipeline_color_blend_attachment_state()};

        std::vector<VkFormat> color_formats = {PixelFormatToVulkan(VisibilityBufferFormat)};
        const VkFormat        depth_format = PixelFormatToVulkan(MainPassDepthFormat);

        VkPipelineCreationFeedback              feedback = {};
        std::vector<VkPipelineCreationFeedback> feedback_stages(shader_stages.size());
        VkPipelineCreationFeedbackCreateInfo    feedback_info = {
               .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,
               .pNext = nullptr,
               .pPipelineCreationFeedback = &feedback,
               .pipelineStageCreationFeedbackCount = static_cast<u32>(feedback_stages.size()),
               .pPipelineStageCreationFeedbacks = feedback_stages.data(),
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

VisibilityBufferPassResources create_vis_buffer_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                               const ShaderModules& shader_modules)
{
    VisibilityBufferPassResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };

        const VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(backend.device, bindings);

        resources.pipe.desc_set_layout = descriptor_set_layout;

        resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptor_set_layout, 1));

        resources.pipe.pipeline =
            create_vis_buffer_pipeline(root, backend, resources.pipe.pipelineLayout, shader_modules);
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {Slot_VisBuffer, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_GBuffer0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_GBuffer1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_instance_params, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_visible_index_buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_buffer_position_ms, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_buffer_normal_ms, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_buffer_uv, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_visible_meshlets, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_diffuse_map_sampler, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_diffuse_maps, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount, VK_SHADER_STAGE_COMPUTE_BIT,
             nullptr},
        };

        std::vector<VkDescriptorBindingFlags> binding_flags(bindings.size(), VK_FLAGS_NONE);
        binding_flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, bindings, binding_flags);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(FillGBufferPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(
            backend.device, nonstd::span(&descriptor_set_layout, 1), nonstd::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipelineLayout, shader_modules.vis_fill_gbuffer_cs);

        resources.fill_pipe.desc_set_layout = descriptor_set_layout;
        resources.fill_pipe.pipelineLayout = pipelineLayout;
        resources.fill_pipe.pipeline = pipeline;
    }

    resources.instancesConstantBuffer =
        create_buffer(root, backend.device, "VisibilityBuffer Instance buffer",
                      DefaultGPUBufferProperties(VisibilityBufferInstanceCountMax, sizeof(ForwardInstanceParams),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, resources.pipe.desc_set_layout,
                             nonstd::span(&resources.descriptor_set, 1));

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, resources.fill_pipe.desc_set_layout,
                             nonstd::span(&resources.descriptor_set_fill, 1));
    return resources;
}

void destroy_vis_buffer_pass_resources(VulkanBackend& backend, VisibilityBufferPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.desc_set_layout, nullptr);

    vkDestroyPipeline(backend.device, resources.fill_pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.fill_pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.fill_pipe.desc_set_layout, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.instancesConstantBuffer.handle,
                     resources.instancesConstantBuffer.allocation);
}

void update_vis_buffer_pass_descriptor_sets(DescriptorWriteHelper&               write_helper,
                                            const VisibilityBufferPassResources& resources,
                                            const PreparedData&                  prepared,
                                            const SamplerResources&              sampler_resources,
                                            const MaterialResources&             material_resources,
                                            const MeshletCullingResources&       meshlet_culling_resources,
                                            const MeshCache&                     mesh_cache,
                                            const FrameGraphTexture&             vis_buffer,
                                            const FrameGraphTexture&             gbuffer_rt0,
                                            const FrameGraphTexture&             gbuffer_rt1)
{
    const GPUBufferView visible_index_buffer_view =
        get_buffer_view(meshlet_culling_resources.visible_index_buffer.properties_deprecated,
                        get_meshlet_visible_index_buffer_pass(prepared.main_culling_pass_index));

    write_helper.append(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.instancesConstantBuffer.handle);
    write_helper.append(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        meshlet_culling_resources.visible_meshlet_buffer.handle);
    write_helper.append(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferPosition.handle);

    write_helper.append(resources.descriptor_set_fill, Slot_VisBuffer, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        vis_buffer.default_view_handle, vis_buffer.image_layout);
    write_helper.append(resources.descriptor_set_fill, Slot_GBuffer0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        gbuffer_rt0.default_view_handle, gbuffer_rt0.image_layout);
    write_helper.append(resources.descriptor_set_fill, Slot_GBuffer1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        gbuffer_rt1.default_view_handle, gbuffer_rt1.image_layout);
    write_helper.append(resources.descriptor_set_fill, Slot_instance_params, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.instancesConstantBuffer.handle);
    write_helper.append(resources.descriptor_set_fill, Slot_visible_index_buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        meshlet_culling_resources.visible_index_buffer.handle, visible_index_buffer_view.offset_bytes,
                        visible_index_buffer_view.size_bytes);
    write_helper.append(resources.descriptor_set_fill, Slot_buffer_position_ms, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferPosition.handle);
    write_helper.append(resources.descriptor_set_fill, Slot_buffer_normal_ms, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferNormal.handle);
    write_helper.append(resources.descriptor_set_fill, Slot_buffer_uv, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferUV.handle);
    write_helper.append(resources.descriptor_set_fill, Slot_visible_meshlets, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        meshlet_culling_resources.visible_meshlet_buffer.handle);
    write_helper.append(resources.descriptor_set_fill, Slot_diffuse_map_sampler, sampler_resources.diffuse_map_sampler);

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
            resources.descriptor_set_fill, Slot_diffuse_maps, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, albedo_image_infos));
    }
}

void upload_vis_buffer_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                            VisibilityBufferPassResources& pass_resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.forward_instances.empty())
        return;

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, pass_resources.instancesConstantBuffer,
                                  prepared.forward_instances.data(),
                                  prepared.forward_instances.size() * sizeof(ForwardInstanceParams));
}

void record_vis_buffer_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                           const VisibilityBufferPassResources& pass_resources,
                                           const MeshletCullingResources&       meshlet_culling_resources,
                                           const FrameGraphTexture& vis_buffer, const FrameGraphTexture& depth_buffer)
{
    if (prepared.forward_instances.empty())
        return;

    const VkExtent2D extent = {depth_buffer.properties.width, depth_buffer.properties.height};
    const VkRect2D   pass_rect = default_vk_rect(extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo vis_buffer_attachment =
        default_rendering_attachment_info(vis_buffer.default_view_handle, vis_buffer.image_layout);
    vis_buffer_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    vis_buffer_attachment.clearValue = VkClearValue{.color{.uint32 = {0, 0}}};

    std::vector<VkRenderingAttachmentInfo> color_attachments = {vis_buffer_attachment};

    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_buffer.default_view_handle, depth_buffer.image_layout);
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.clearValue = VkClearDepthStencil(MainPassUseReverseZ ? 0.f : 1.f, 0);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, color_attachments, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    const MeshletDrawParams draw_params = get_meshlet_draw_params(prepared.main_culling_pass_index);

    std::vector<VkDescriptorSet> pass_descriptors = {
        pass_resources.descriptor_set,
    };

    vkCmdBindIndexBuffer(cmdBuffer.handle, meshlet_culling_resources.visible_index_buffer.handle,
                         draw_params.index_buffer_offset, draw_params.index_type);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipelineLayout, 0,
                            static_cast<u32>(pass_descriptors.size()), pass_descriptors.data(), 0, nullptr);

    vkCmdDrawIndexedIndirectCount(
        cmdBuffer.handle, meshlet_culling_resources.visible_indirect_draw_commands_buffer.handle,
        draw_params.command_buffer_offset, meshlet_culling_resources.counters_buffer.handle,
        draw_params.counter_buffer_offset, draw_params.command_buffer_max_count,
        meshlet_culling_resources.visible_indirect_draw_commands_buffer.properties_deprecated.element_size_bytes);

    vkCmdEndRendering(cmdBuffer.handle);
}

void record_fill_gbuffer_pass_command_buffer(CommandBuffer&                       cmdBuffer,
                                             const VisibilityBufferPassResources& resources,
                                             VkExtent2D                           backbufferExtent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.fill_pipe.pipeline);

    FillGBufferPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(backbufferExtent.width, backbufferExtent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(backbufferExtent.width), 1.f / static_cast<float>(backbufferExtent.height));

    vkCmdPushConstants(cmdBuffer.handle, resources.fill_pipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.fill_pipe.pipelineLayout, 0, 1,
                            &resources.descriptor_set_fill, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(backbufferExtent.width, GBufferFillThreadCountX),
                  div_round_up(backbufferExtent.height, GBufferFillThreadCountY),
                  1);
}
} // namespace Reaper
