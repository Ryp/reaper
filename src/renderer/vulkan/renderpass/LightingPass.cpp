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
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/Shader.h"

#include "common/ReaperRoot.h"
#include "profiling/Scope.h"

#include "renderer/shader/share/tile_depth.hlsl"

namespace Reaper
{
constexpr u32 PointLightMax = 128;

LightingPassResources create_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend)
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

        VkShaderModule shader =
            vulkan_create_shader_module(backend.device, "build/shader/tile_depth_downsample.comp.spv");

        VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, nonstd::span(&descriptor_set_layout, 1), nonstd::span(&pushConstantRange, 1));

        VkPipeline pipeline = create_compute_pipeline(backend.device, pipeline_layout, shader);

        vkDestroyShaderModule(backend.device, shader, nullptr);

        resources.tile_depth_descriptor_set_layout = descriptor_set_layout;
        resources.tile_depth_pipeline_layout = pipeline_layout;
        resources.tile_depth_pipeline = pipeline;

        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1,
                                                              &descriptor_set_layout};

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &resources.tile_depth_descriptor_set)
               == VK_SUCCESS);
    }

    resources.pointLightBuffer = create_buffer(
        root, backend.device, "Point light buffer",
        DefaultGPUBufferProperties(PointLightMax, sizeof(PointLightProperties), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    return resources;
}

void destroy_lighting_pass_resources(VulkanBackend& backend, LightingPassResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.pointLightBuffer.handle, resources.pointLightBuffer.allocation);

    vkDestroyPipeline(backend.device, resources.tile_depth_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.tile_depth_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tile_depth_descriptor_set_layout, nullptr);
}

void update_lighting_pass_pass_descriptor_set(VulkanBackend& backend, const LightingPassResources& resources,
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
} // namespace Reaper
