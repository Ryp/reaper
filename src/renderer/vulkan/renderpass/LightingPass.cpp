////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "LightingPass.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "common/ReaperRoot.h"
#include "profiling/Scope.h"

#include "renderer/shader/share/tile_depth.hlsl"

namespace Reaper
{
constexpr u32 PointLightMax = 128;

LightingPassResources create_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                     const ShaderModules& shader_modules)
{
    LightingPassResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TileDepthConstants)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, nonstd::span(&descriptor_set_layout, 1), nonstd::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.tile_depth_downsample_cs);

        resources.tile_depth_descriptor_set_layout = descriptor_set_layout;
        resources.tile_depth_pipeline_layout = pipeline_layout;
        resources.tile_depth_pipeline = pipeline;
    }

    {
        const char* entry_point = "main";

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
             shader_modules.fullscreen_triangle_vs, entry_point, nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             shader_modules.copy_to_depth_fs, entry_point, nullptr}};
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        VkPipelineLayout pipeline_layout =
            create_pipeline_layout(backend.device, nonstd::span(&descriptor_set_layout, 1));

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = PixelFormatToVulkan(PixelFormat::D16_UNORM);

        VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

        resources.depth_copy_descriptor_set_layout = descriptor_set_layout;
        resources.depth_copy_pipeline_layout = pipeline_layout;
        resources.depth_copy_pipeline = pipeline;
    }

    std::vector<VkDescriptorSetLayout> dset_layouts = {resources.tile_depth_descriptor_set_layout,
                                                       resources.depth_copy_descriptor_set_layout,
                                                       resources.depth_copy_descriptor_set_layout};
    std::vector<VkDescriptorSet>       dsets(dset_layouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

    resources.tile_depth_descriptor_set = dsets[0];
    resources.depth_copy_descriptor_sets[0] = dsets[1];
    resources.depth_copy_descriptor_sets[1] = dsets[2];

    resources.pointLightBuffer = create_buffer(
        root, backend.device, "Point light buffer",
        DefaultGPUBufferProperties(PointLightMax, sizeof(PointLightProperties), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    return resources;
}

void destroy_lighting_pass_resources(VulkanBackend& backend, LightingPassResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.pointLightBuffer.handle, resources.pointLightBuffer.allocation);

    vkDestroyPipeline(backend.device, resources.depth_copy_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.depth_copy_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.depth_copy_descriptor_set_layout, nullptr);

    vkDestroyPipeline(backend.device, resources.tile_depth_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.tile_depth_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tile_depth_descriptor_set_layout, nullptr);
}

void update_lighting_pass_descriptor_set(VulkanBackend& backend, const LightingPassResources& resources,
                                         const SamplerResources& sampler_resources, VkImageView scene_depth_view,
                                         VkImageView tile_depth_min_view, VkImageView tile_depth_max_view)
{
    const VkDescriptorImageInfo descSampler = {sampler_resources.linearClampSampler, VK_NULL_HANDLE,
                                               VK_IMAGE_LAYOUT_UNDEFINED};
    const VkDescriptorImageInfo descTexture = {VK_NULL_HANDLE, scene_depth_view,
                                               VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};
    const VkDescriptorImageInfo descTileMin = {VK_NULL_HANDLE, tile_depth_min_view, VK_IMAGE_LAYOUT_GENERAL};
    const VkDescriptorImageInfo descTileMax = {VK_NULL_HANDLE, tile_depth_max_view, VK_IMAGE_LAYOUT_GENERAL};

    std::vector<VkWriteDescriptorSet> descriptorSetWrites = {
        create_image_descriptor_write(resources.tile_depth_descriptor_set, 0, VK_DESCRIPTOR_TYPE_SAMPLER, &descSampler),
        create_image_descriptor_write(resources.tile_depth_descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                      &descTexture),
        create_image_descriptor_write(resources.tile_depth_descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                      &descTileMin),
        create_image_descriptor_write(resources.tile_depth_descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                      &descTileMax),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(descriptorSetWrites.size()), descriptorSetWrites.data(), 0,
                           nullptr);
}

void update_depth_copy_pass_descriptor_set(VulkanBackend& backend, const LightingPassResources& resources,
                                           VkImageView depth_min_src, VkImageView depth_max_src)
{
    const VkDescriptorImageInfo descTileMin = {VK_NULL_HANDLE, depth_min_src, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    const VkDescriptorImageInfo descTileMax = {VK_NULL_HANDLE, depth_max_src, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};

    std::vector<VkWriteDescriptorSet> writes = {
        create_image_descriptor_write(resources.depth_copy_descriptor_sets[0], 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                      &descTileMin),
        create_image_descriptor_write(resources.depth_copy_descriptor_sets[1], 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                      &descTileMax),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

void upload_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                          LightingPassResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.point_lights.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, resources.pointLightBuffer, prepared.point_lights.data(),
                       prepared.point_lights.size() * sizeof(PointLightProperties));
}

void record_tile_depth_pass_command_buffer(CommandBuffer& cmdBuffer, const LightingPassResources& resources,
                                           VkExtent2D backbufferExtent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tile_depth_pipeline);

    TileDepthConstants push_constants;
    push_constants.extent_ts = glm::uvec2(backbufferExtent.width, backbufferExtent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(backbufferExtent.width), 1.f / static_cast<float>(backbufferExtent.height));

    vkCmdPushConstants(cmdBuffer.handle, resources.tile_depth_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tile_depth_pipeline_layout, 0,
                            1, &resources.tile_depth_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(backbufferExtent.width, TileDepthThreadCountX * 2),
                  div_round_up(backbufferExtent.height, TileDepthThreadCountY * 2),
                  1);
}

void record_depth_copy(CommandBuffer& cmdBuffer, const LightingPassResources& resources, VkImageView depth_min_dst,
                       VkImageView depth_max_dst, VkExtent2D depth_extent)
{
    std::vector<VkImageView> depth_dsts = {depth_min_dst, depth_max_dst};
    const VkRect2D           pass_rect = default_vk_rect(depth_extent);
    const VkViewport         viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.depth_copy_pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    for (u32 depth_index = 0; depth_index < 2; depth_index++)
    {
        const VkRenderingAttachmentInfo depth_attachment = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            nullptr,
            depth_dsts[depth_index],
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_RESOLVE_MODE_NONE,
            VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VkClearValue{},
        };

        const VkRenderingInfo rendering_info = {
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

        vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.depth_copy_pipeline_layout,
                                0, 1, &resources.depth_copy_descriptor_sets[depth_index], 0, nullptr);

        vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer.handle);
    }
}
} // namespace Reaper
