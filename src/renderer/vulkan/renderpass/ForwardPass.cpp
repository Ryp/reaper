////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ForwardPass.h"

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
#include "renderer/vulkan/renderpass/ForwardPassConstants.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/shader/forward.share.hlsl"

namespace Reaper
{
constexpr u32 ForwardInstanceCountMax = 512;

namespace
{
    VkPipeline create_forward_pipeline(ReaperRoot& root, VulkanBackend& backend, VkPipelineLayout pipeline_layout,
                                       const ShaderModules& shader_modules)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shader_modules.forward_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shader_modules.forward_fs)};

        const VkPipelineColorBlendAttachmentState blend_attachment_state =
            default_pipeline_color_blend_attachment_state();

        const VkFormat color_format = PixelFormatToVulkan(ForwardHDRColorFormat);
        const VkFormat depth_format = PixelFormatToVulkan(MainPassDepthFormat);

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.input_assembly.primitiveRestartEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthCompareOp =
            MainPassUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        pipeline_properties.blend_state.attachmentCount = 1;
        pipeline_properties.blend_state.pAttachments = &blend_attachment_state;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = 1;
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = &color_format;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = depth_format;

        VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

        log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

        return pipeline;
    }
} // namespace

ForwardPassResources create_forward_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                   const ShaderModules& shader_modules)
{
    ForwardPassResources resources = {};

    std::vector<VkDescriptorSetLayoutBinding> bindings0 = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {7, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {8, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ShadowMapMaxCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    std::vector<VkDescriptorBindingFlags> bindingFlags0(bindings0.size(), VK_FLAGS_NONE);
    bindingFlags0[8] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings1 = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    std::vector<VkDescriptorBindingFlags> bindingFlags1 = {VK_FLAGS_NONE, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
        create_descriptor_set_layout(backend.device, bindings0, bindingFlags0),
        create_descriptor_set_layout(backend.device, bindings1, bindingFlags1),
    };

    resources.pipe.desc_set_layout = descriptorSetLayouts[0];
    resources.pipe.desc_set_layout_material = descriptorSetLayouts[1];

    resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, descriptorSetLayouts);

    resources.pipe.pipeline = create_forward_pipeline(root, backend, resources.pipe.pipelineLayout, shader_modules);

    resources.pass_constant_buffer =
        create_buffer(root, backend.device, "Forward Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(ForwardPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.instance_buffer =
        create_buffer(root, backend.device, "Forward Instance buffer",
                      DefaultGPUBufferProperties(ForwardInstanceCountMax, sizeof(ForwardInstanceParams),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    std::vector<VkDescriptorSet> dsets(descriptorSetLayouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, descriptorSetLayouts, dsets);

    resources.descriptor_set = dsets[0];
    resources.material_descriptor_set = dsets[1];

    return resources;
}

void destroy_forward_pass_resources(VulkanBackend& backend, ForwardPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.desc_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.desc_set_layout_material, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.pass_constant_buffer.handle,
                     resources.pass_constant_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.instance_buffer.handle, resources.instance_buffer.allocation);
}

void update_forward_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const ForwardPassResources& resources,
                                         const FrameGraphBuffer&  visible_meshlet_buffer,
                                         const SamplerResources&  sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                         const LightingPassResources&       lighting_resources,
                                         std::span<const FrameGraphTexture> shadow_maps)
{
    write_helper.append(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        resources.pass_constant_buffer.handle);
    write_helper.append(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.instance_buffer.handle);
    write_helper.append(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, visible_meshlet_buffer.handle);
    write_helper.append(resources.descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferPosition.handle);
    write_helper.append(resources.descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferNormal.handle);
    write_helper.append(resources.descriptor_set, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_cache.vertexBufferUV.handle);
    write_helper.append(resources.descriptor_set, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        lighting_resources.pointLightBuffer.handle);
    write_helper.append(resources.descriptor_set, 7, sampler_resources.shadow_map_sampler);

    if (!shadow_maps.empty())
    {
        std::span<VkDescriptorImageInfo> shadow_map_image_infos = write_helper.new_image_infos(shadow_maps.size());

        for (u32 index = 0; index < shadow_maps.size(); index += 1)
        {
            const auto& shadow_map = shadow_maps[index];
            shadow_map_image_infos[index] =
                create_descriptor_image_info(shadow_map.default_view_handle, shadow_map.image_layout);
        }

        write_helper.writes.push_back(create_image_descriptor_write(
            resources.descriptor_set, 8, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_map_image_infos));
    }

    if (!material_resources.texture_handles.empty())
    {
        write_helper.append(resources.material_descriptor_set, 0, sampler_resources.diffuse_map_sampler);

        std::span<VkDescriptorImageInfo> albedo_image_infos =
            write_helper.new_image_infos(material_resources.texture_handles.size());

        for (u32 index = 0; index < albedo_image_infos.size(); index += 1)
        {
            const auto& albedo_map = material_resources.textures[index];
            albedo_image_infos[index] =
                create_descriptor_image_info(albedo_map.default_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        write_helper.writes.push_back(create_image_descriptor_write(
            resources.material_descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, albedo_image_infos));
    }
}

void upload_forward_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                         ForwardPassResources& pass_resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.forward_instances.empty())
        return;

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, pass_resources.pass_constant_buffer,
                                  &prepared.forward_pass_constants, sizeof(ForwardPassParams));

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, pass_resources.instance_buffer,
                                  prepared.forward_instances.data(),
                                  prepared.forward_instances.size() * sizeof(ForwardInstanceParams));
}

void record_forward_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                        const ForwardPassResources& pass_resources,
                                        const FrameGraphBuffer&     meshlet_counters,
                                        const FrameGraphBuffer&     meshlet_indirect_draw_commands,
                                        const FrameGraphBuffer&     meshlet_visible_index_buffer,
                                        const FrameGraphTexture& hdr_buffer, const FrameGraphTexture& depth_buffer)
{
    if (prepared.forward_instances.empty())
        return;

    const VkExtent2D extent = {hdr_buffer.properties.width, hdr_buffer.properties.height};
    const VkRect2D   pass_rect = default_vk_rect(extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo color_attachment =
        default_rendering_attachment_info(hdr_buffer.default_view_handle, hdr_buffer.image_layout);
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.clearValue = VkClearColor({0.1f, 0.1f, 0.1f, 0.0f});

    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_buffer.default_view_handle, depth_buffer.image_layout);
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.clearValue = VkClearDepthStencil(MainPassUseReverseZ ? 0.f : 1.f, 0);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, &color_attachment, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    const MeshletDrawParams meshlet_draw = get_meshlet_draw_params(prepared.main_culling_pass_index);

    vkCmdBindIndexBuffer(cmdBuffer.handle, meshlet_visible_index_buffer.handle, meshlet_draw.index_buffer_offset,
                         meshlet_draw.index_type);

    std::array<VkDescriptorSet, 2> pass_descriptors = {
        pass_resources.descriptor_set,
        pass_resources.material_descriptor_set,
    };

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipelineLayout, 0,
                            static_cast<u32>(pass_descriptors.size()), pass_descriptors.data(), 0, nullptr);

    vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, meshlet_indirect_draw_commands.handle,
                                  meshlet_draw.command_buffer_offset, meshlet_counters.handle,
                                  meshlet_draw.counter_buffer_offset, meshlet_draw.command_buffer_max_count,
                                  meshlet_indirect_draw_commands.properties.element_size_bytes);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
