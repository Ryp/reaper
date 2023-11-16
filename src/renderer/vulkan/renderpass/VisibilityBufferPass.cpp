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
#include "renderer/vulkan/StorageBufferAllocator.h"
#include "renderer/vulkan/renderpass/Constants.h"
#include "renderer/vulkan/renderpass/VisibilityBufferConstants.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/shader/vis_buffer/fill_gbuffer.share.hlsl"

#include <vector>

namespace Reaper
{
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
        pipeline_properties.blend_state.attachmentCount = static_cast<u32>(blend_attachment_state.size());
        pipeline_properties.blend_state.pAttachments = blend_attachment_state.data();
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = static_cast<u32>(color_formats.size());
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = color_formats.data();
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = depth_format;

        VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

        Assert(backend.physical_device.graphics_queue_family_index
               == backend.physical_device.present_queue_family_index);

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

        resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1));

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
            {Slot_buffer_tangent_ms, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_buffer_uv, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_visible_meshlets, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_diffuse_map_sampler, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {Slot_material_maps, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MaterialTextureMaxCount, VK_SHADER_STAGE_COMPUTE_BIT,
             nullptr},
        };

        std::vector<VkDescriptorBindingFlags> binding_flags(bindings.size(), VK_FLAGS_NONE);
        binding_flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, bindings, binding_flags);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(FillGBufferPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1),
                                                                 std::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipelineLayout, shader_modules.vis_fill_gbuffer_cs);

        resources.fill_pipe.desc_set_layout = descriptor_set_layout;
        resources.fill_pipe.pipelineLayout = pipelineLayout;
        resources.fill_pipe.pipeline = pipeline;
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.pipe.desc_set_layout, 1), std::span(&resources.descriptor_set, 1));

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.fill_pipe.desc_set_layout, 1),
                             std::span(&resources.descriptor_set_fill, 1));
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
}

void update_vis_buffer_pass_resources(const FrameGraph::FrameGraph&        frame_graph,
                                      const FrameGraphResources&           frame_graph_resources,
                                      const VisBufferFrameGraphRecord&     record,
                                      DescriptorWriteHelper&               write_helper,
                                      StorageBufferAllocator&              frame_storage_allocator,
                                      const VisibilityBufferPassResources& resources,
                                      const PreparedData&                  prepared,
                                      const SamplerResources&              sampler_resources,
                                      const MaterialResources&             material_resources,
                                      const MeshCache&                     mesh_cache)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.mesh_instances.empty())
        return;

    Assert(!prepared.mesh_instances.empty());

    StorageBufferAlloc mesh_instance_alloc =
        allocate_storage(frame_storage_allocator, prepared.mesh_instances.size() * sizeof(MeshInstance));

    upload_storage_buffer(frame_storage_allocator, mesh_instance_alloc, prepared.mesh_instances.data());

    {
        const FrameGraphBuffer visible_meshlet_buffer =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, record.render.visible_meshlet_buffer);

        write_helper.append(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_instance_alloc.buffer,
                            mesh_instance_alloc.offset_bytes, mesh_instance_alloc.size_bytes);
        write_helper.append(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            visible_meshlet_buffer.handle);
        write_helper.append(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            mesh_cache.vertexBufferPosition.handle);
    }

    {
        const FrameGraphBuffer meshlet_visible_index_buffer = get_frame_graph_buffer(
            frame_graph_resources, frame_graph, record.fill_gbuffer.meshlet_visible_index_buffer);
        const FrameGraphBuffer visible_meshlet_buffer =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, record.fill_gbuffer.visible_meshlet_buffer);
        const FrameGraphTexture vis_buffer =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.fill_gbuffer.vis_buffer);
        const FrameGraphTexture gbuffer_rt0 =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.fill_gbuffer.gbuffer_rt0);
        const FrameGraphTexture gbuffer_rt1 =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.fill_gbuffer.gbuffer_rt1);

        const GPUBufferView visible_index_buffer_view =
            get_buffer_view(meshlet_visible_index_buffer.properties,
                            get_meshlet_visible_index_buffer_pass(prepared.main_culling_pass_index));

        write_helper.append(resources.descriptor_set_fill, Slot_VisBuffer, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                            vis_buffer.default_view_handle, vis_buffer.image_layout);
        write_helper.append(resources.descriptor_set_fill, Slot_GBuffer0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            gbuffer_rt0.default_view_handle, gbuffer_rt0.image_layout);
        write_helper.append(resources.descriptor_set_fill, Slot_GBuffer1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            gbuffer_rt1.default_view_handle, gbuffer_rt1.image_layout);
        write_helper.append(resources.descriptor_set_fill, Slot_instance_params, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            mesh_instance_alloc.buffer, mesh_instance_alloc.offset_bytes,
                            mesh_instance_alloc.size_bytes);
        write_helper.append(resources.descriptor_set_fill, Slot_visible_index_buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            meshlet_visible_index_buffer.handle, visible_index_buffer_view.offset_bytes,
                            visible_index_buffer_view.size_bytes);
        write_helper.append(resources.descriptor_set_fill, Slot_buffer_position_ms, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            mesh_cache.vertexBufferPosition.handle);
        write_helper.append(resources.descriptor_set_fill, Slot_buffer_normal_ms, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            mesh_cache.vertexBufferNormal.handle);
        write_helper.append(resources.descriptor_set_fill, Slot_buffer_tangent_ms, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            mesh_cache.vertexBufferTangent.handle);
        write_helper.append(resources.descriptor_set_fill, Slot_buffer_uv, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            mesh_cache.vertexBufferUV.handle);
        write_helper.append(resources.descriptor_set_fill, Slot_visible_meshlets, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            visible_meshlet_buffer.handle);
        write_helper.append(resources.descriptor_set_fill, Slot_diffuse_map_sampler,
                            sampler_resources.diffuse_map_sampler);

        if (!material_resources.textures.empty())
        {
            std::span<VkDescriptorImageInfo> albedo_image_infos =
                write_helper.new_image_infos(static_cast<u32>(material_resources.textures.size()));

            for (u32 index = 0; index < albedo_image_infos.size(); index += 1)
            {
                const auto& albedo_map = material_resources.textures[index];
                albedo_image_infos[index] =
                    create_descriptor_image_info(albedo_map.default_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            write_helper.writes.push_back(
                create_image_descriptor_write(resources.descriptor_set_fill, Slot_material_maps,
                                              VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, albedo_image_infos));
        }
    }
}

void record_vis_buffer_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                           const VisibilityBufferPassResources& pass_resources,
                                           const FrameGraphBuffer&              meshlet_counters,
                                           const FrameGraphBuffer&              meshlet_indirect_draw_commands,
                                           const FrameGraphBuffer&              meshlet_visible_index_buffer,
                                           const FrameGraphTexture& vis_buffer, const FrameGraphTexture& depth_buffer)
{
    if (prepared.mesh_instances.empty())
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

    const MeshletDrawParams meshlet_draw = get_meshlet_draw_params(prepared.main_culling_pass_index);

    std::vector<VkDescriptorSet> pass_descriptors = {
        pass_resources.descriptor_set,
    };

    vkCmdBindIndexBuffer(cmdBuffer.handle, meshlet_visible_index_buffer.handle, meshlet_draw.index_buffer_offset,
                         meshlet_draw.index_type);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipelineLayout, 0,
                            static_cast<u32>(pass_descriptors.size()), pass_descriptors.data(), 0, nullptr);

    vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, meshlet_indirect_draw_commands.handle,
                                  meshlet_draw.command_buffer_offset, meshlet_counters.handle,
                                  meshlet_draw.counter_buffer_offset, meshlet_draw.command_buffer_max_count,
                                  meshlet_indirect_draw_commands.properties.element_size_bytes);

    vkCmdEndRendering(cmdBuffer.handle);
}

void record_fill_gbuffer_pass_command_buffer(CommandBuffer&                       cmdBuffer,
                                             const VisibilityBufferPassResources& resources,
                                             VkExtent2D                           render_extent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.fill_pipe.pipeline);

    FillGBufferPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(render_extent.width, render_extent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(render_extent.width), 1.f / static_cast<float>(render_extent.height));

    vkCmdPushConstants(cmdBuffer.handle, resources.fill_pipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.fill_pipe.pipelineLayout, 0, 1,
                            &resources.descriptor_set_fill, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(render_extent.width, GBufferFillThreadCountX),
                  div_round_up(render_extent.height, GBufferFillThreadCountY),
                  1);
}
} // namespace Reaper
