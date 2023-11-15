////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TiledLightingPass.h"

#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/buffer/GPUBufferView.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/StorageBufferAllocator.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "profiling/Scope.h"

#include "renderer/shader/tiled_lighting/tiled_lighting.share.hlsl"

namespace Reaper
{
TiledLightingPassResources create_tiled_lighting_pass_resources(VulkanBackend&       backend,
                                                                const ShaderModules& shader_modules)
{
    TiledLightingPassResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings0 = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {9, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ShadowMapMaxCount, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags0(bindings0.size(), VK_FLAGS_NONE);
        bindingFlags0[9] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        std::vector<VkDescriptorSetLayout> descriptor_set_layouts = {
            create_descriptor_set_layout(backend.device, bindings0, bindingFlags0)};

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(TiledLightingPushConstants)};

        VkPipelineLayout pipeline_layout =
            create_pipeline_layout(backend.device, descriptor_set_layouts, std::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.tiled_lighting_cs);

        resources.tiled_lighting_descriptor_set_layout = descriptor_set_layouts[0];
        resources.tiled_lighting_pipeline_layout = pipeline_layout;
        resources.tiled_lighting_pipeline = pipeline;
    }

    {
        std::vector<VkDescriptorSetLayout> dset_layouts = {resources.tiled_lighting_descriptor_set_layout};
        std::vector<VkDescriptorSet>       dsets(dset_layouts.size());

        allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

        resources.tiled_lighting_descriptor_set = dsets[0];
    }

    resources.tiled_lighting_constant_buffer =
        create_buffer(backend.device, "Tiled lighting constants",
                      DefaultGPUBufferProperties(1, sizeof(TiledLightingConstants), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    // Debug
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(backend.device, bindings);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(TiledLightingDebugPushConstants)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1),
                                                                  std::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.tiled_lighting_debug_cs);

        resources.tiled_lighting_debug_descriptor_set_layout = descriptor_set_layout;
        resources.tiled_lighting_debug_pipeline_layout = pipeline_layout;
        resources.tiled_lighting_debug_pipeline = pipeline;
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.tiled_lighting_debug_descriptor_set_layout, 1),
                             std::span(&resources.tiled_lighting_debug_descriptor_set, 1));

    return resources;
}

void destroy_tiled_lighting_pass_resources(VulkanBackend& backend, TiledLightingPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.tiled_lighting_debug_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.tiled_lighting_debug_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tiled_lighting_debug_descriptor_set_layout, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.tiled_lighting_constant_buffer.handle,
                     resources.tiled_lighting_constant_buffer.allocation);

    vkDestroyPipeline(backend.device, resources.tiled_lighting_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.tiled_lighting_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tiled_lighting_descriptor_set_layout, nullptr);
}

void update_tiled_lighting_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                                const LightingPassResources&      lighting_resources,
                                                const TiledLightingPassResources& resources,
                                                const SamplerResources&           sampler_resources,
                                                const FrameGraphBuffer&           light_list_buffer,
                                                const FrameGraphTexture&          gbuffer_rt0,
                                                const FrameGraphTexture&          gbuffer_rt1,
                                                const FrameGraphTexture&          main_view_depth,
                                                const FrameGraphTexture&          lighting_output,
                                                const FrameGraphBuffer&           tile_debug_buffer,
                                                std::span<const FrameGraphTexture>
                                                    shadow_maps)
{
    VkDescriptorSet dset = resources.tiled_lighting_descriptor_set;
    write_helper.append(dset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, resources.tiled_lighting_constant_buffer.handle);
    write_helper.append(dset, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_list_buffer.handle);
    write_helper.append(dset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gbuffer_rt0.default_view_handle,
                        gbuffer_rt0.image_layout);
    write_helper.append(dset, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gbuffer_rt1.default_view_handle,
                        gbuffer_rt1.image_layout);
    write_helper.append(dset, 4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, main_view_depth.default_view_handle,
                        main_view_depth.image_layout);
    write_helper.append(dset, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lighting_resources.point_light_buffer_alloc.buffer,
                        lighting_resources.point_light_buffer_alloc.offset_bytes,
                        lighting_resources.point_light_buffer_alloc.size_bytes);
    write_helper.append(dset, 6, sampler_resources.shadow_map_sampler);
    write_helper.append(dset, 7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, lighting_output.default_view_handle,
                        lighting_output.image_layout);
    write_helper.append(dset, 8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tile_debug_buffer.handle);

    if (!shadow_maps.empty())
    {
        std::span<VkDescriptorImageInfo> shadow_map_image_infos =
            write_helper.new_image_infos(static_cast<u32>(shadow_maps.size()));

        for (u32 index = 0; index < shadow_maps.size(); index += 1)
        {
            const auto& shadow_map = shadow_maps[index];
            shadow_map_image_infos[index] =
                create_descriptor_image_info(shadow_map.default_view_handle, shadow_map.image_layout);
        }

        write_helper.writes.push_back(
            create_image_descriptor_write(dset, 9, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_map_image_infos));
    }
}

void upload_tiled_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                                TiledLightingPassResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.point_lights.empty())
        return;

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.tiled_lighting_constant_buffer,
                                  &prepared.tiled_light_constants, sizeof(TiledLightingConstants));
}

void record_tiled_lighting_command_buffer(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                                          VkExtent2D render_extent, VkExtent2D tile_extent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tiled_lighting_pipeline);

    TiledLightingPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(render_extent.width, render_extent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(render_extent.width), 1.f / static_cast<float>(render_extent.height));
    push_constants.tile_count_x = tile_extent.width;

    vkCmdPushConstants(cmdBuffer.handle, resources.tiled_lighting_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tiled_lighting_pipeline_layout,
                            0, 1, &resources.tiled_lighting_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(render_extent.width, TiledLightingThreadCountX),
                  div_round_up(render_extent.height, TiledLightingThreadCountY),
                  1);
}

void update_tiled_lighting_debug_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                                      const TiledLightingPassResources& resources,
                                                      const FrameGraphBuffer&           tile_debug_buffer,
                                                      const FrameGraphTexture&          tile_debug_texture)
{
    VkDescriptorSet dset = resources.tiled_lighting_debug_descriptor_set;
    write_helper.append(dset, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tile_debug_buffer.handle);
    write_helper.append(dset, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, tile_debug_texture.default_view_handle,
                        tile_debug_texture.image_layout);
}

void record_tiled_lighting_debug_command_buffer(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                                                VkExtent2D render_extent, VkExtent2D tile_extent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tiled_lighting_debug_pipeline);

    TiledLightingDebugPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(render_extent.width, render_extent.height);
    push_constants.tile_count_x = tile_extent.width;

    vkCmdPushConstants(cmdBuffer.handle, resources.tiled_lighting_debug_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            resources.tiled_lighting_debug_pipeline_layout, 0, 1,
                            &resources.tiled_lighting_debug_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(render_extent.width, TiledLightingThreadCountX),
                  div_round_up(render_extent.height, TiledLightingThreadCountY),
                  1);
}
} // namespace Reaper
