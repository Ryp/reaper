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
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "common/ReaperRoot.h"
#include "profiling/Scope.h"

#include "renderer/shader/share/tiled_lighting.hlsl"

namespace Reaper
{
TiledLightingPassResources create_tiled_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                                const ShaderModules& shader_modules)
{
    TiledLightingPassResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings0 = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ShadowMapMaxCount, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags0(bindings0.size(), VK_FLAGS_NONE);
        bindingFlags0[6] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        std::vector<VkDescriptorSetLayoutBinding> bindings1 = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags1 = {VK_FLAGS_NONE,
                                                               VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

        std::vector<VkDescriptorSetLayout> descriptor_set_layouts = {
            create_descriptor_set_layout(backend.device, bindings0, bindingFlags0),
            create_descriptor_set_layout(backend.device, bindings1, bindingFlags1)};

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(TiledLightingPushConstants)};

        VkPipelineLayout pipeline_layout =
            create_pipeline_layout(backend.device, descriptor_set_layouts, nonstd::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.tiled_lighting_cs);

        resources.tiled_lighting_descriptor_set_layout = descriptor_set_layouts[0];
        resources.tiled_lighting_descriptor_set_layout_material = descriptor_set_layouts[1];
        resources.tiled_lighting_pipeline_layout = pipeline_layout;
        resources.tiled_lighting_pipeline = pipeline;
    }

    std::vector<VkDescriptorSetLayout> dset_layouts = {resources.tiled_lighting_descriptor_set_layout,
                                                       resources.tiled_lighting_descriptor_set_layout_material};
    std::vector<VkDescriptorSet>       dsets(dset_layouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

    resources.tiled_lighting_descriptor_set = dsets[0];
    resources.tiled_lighting_descriptor_set_material = dsets[1];

    resources.tiled_lighting_constant_buffer =
        create_buffer(root, backend.device, "Tiled lighting constants",
                      DefaultGPUBufferProperties(1, sizeof(TiledLightingConstants), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    return resources;
}

void destroy_tiled_lighting_pass_resources(VulkanBackend& backend, TiledLightingPassResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.tiled_lighting_constant_buffer.handle,
                     resources.tiled_lighting_constant_buffer.allocation);

    vkDestroyPipeline(backend.device, resources.tiled_lighting_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.tiled_lighting_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tiled_lighting_descriptor_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tiled_lighting_descriptor_set_layout_material, nullptr);
}

void update_tiled_lighting_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                                const LightingPassResources&      lighting_resources,
                                                const TiledLightingPassResources& resources,
                                                const SamplerResources&           sampler_resources,
                                                const FrameGraphBuffer&           light_list_buffer,
                                                const FrameGraphTexture&          main_view_depth,
                                                const FrameGraphTexture&          lighting_output,
                                                nonstd::span<const FrameGraphTexture>
                                                                         shadow_maps,
                                                const MaterialResources& material_resources)
{
    VkDescriptorSet dset0 = resources.tiled_lighting_descriptor_set;
    append_write(write_helper, dset0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 resources.tiled_lighting_constant_buffer.handle);
    append_write(write_helper, dset0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_list_buffer.handle);
    append_write(write_helper, dset0, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, main_view_depth.view_handle,
                 main_view_depth.image_layout);
    append_write(write_helper, dset0, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lighting_resources.pointLightBuffer.handle);
    append_write(write_helper, dset0, 4, sampler_resources.shadowMapSampler);
    append_write(write_helper, dset0, 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, lighting_output.view_handle,
                 lighting_output.image_layout);

    if (!shadow_maps.empty())
    {
        const VkDescriptorImageInfo* image_info_ptr = write_helper.images.data() + write_helper.images.size();
        for (auto shadow_map : shadow_maps)
        {
            write_helper.images.push_back({VK_NULL_HANDLE, shadow_map.view_handle, shadow_map.image_layout});
        }

        nonstd::span<const VkDescriptorImageInfo> shadow_map_image_infos(image_info_ptr, shadow_maps.size());

        write_helper.writes.push_back(
            create_image_descriptor_write(dset0, 6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_map_image_infos));
    }

    if (!material_resources.texture_handles.empty())
    {
        VkDescriptorSet dset1 = resources.tiled_lighting_descriptor_set_material;

        append_write(write_helper, dset1, 0, sampler_resources.diffuseMapSampler);

        const VkDescriptorImageInfo* image_info_ptr = write_helper.images.data() + write_helper.images.size();

        for (auto handle : material_resources.texture_handles)
        {
            write_helper.images.push_back({VK_NULL_HANDLE, material_resources.textures[handle].default_view,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        nonstd::span<const VkDescriptorImageInfo> albedo_image_infos(image_info_ptr,
                                                                     material_resources.texture_handles.size());

        Assert(albedo_image_infos.size() <= DiffuseMapMaxCount);

        write_helper.writes.push_back(
            create_image_descriptor_write(dset1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, albedo_image_infos));
    }
}

void upload_tiled_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                                TiledLightingPassResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.point_lights.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, resources.tiled_lighting_constant_buffer,
                       &prepared.tiled_light_constants, sizeof(TiledLightingConstants));
}

void record_tiled_lighting_command_buffer(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                                          VkExtent2D backbufferExtent, VkExtent2D tile_extent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tiled_lighting_pipeline);

    TiledLightingPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(backbufferExtent.width, backbufferExtent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(backbufferExtent.width), 1.f / static_cast<float>(backbufferExtent.height));
    push_constants.tile_count_x = tile_extent.width;

    vkCmdPushConstants(cmdBuffer.handle, resources.tiled_lighting_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tiled_lighting_pipeline_layout,
                            0, 1, &resources.tiled_lighting_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(backbufferExtent.width, TiledLightingThreadCountX),
                  div_round_up(backbufferExtent.height, TiledLightingThreadCountY),
                  1);
}
} // namespace Reaper
