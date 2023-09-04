////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "HZBPass.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "profiling/Scope.h"

#include "renderer/shader/hzb_reduce.share.hlsl"

namespace Reaper
{
HZBPassResources create_hzb_pass_resources(ReaperRoot& /*root*/, VulkanBackend& backend,
                                           const ShaderModules& shader_modules)
{
    HZBPassResources resources = {};

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {Slot_LinearClampSampler, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {Slot_SceneDepth, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {Slot_HZB_mips, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, HZBMaxMipCount, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    std::vector<VkDescriptorBindingFlags> binding_flags(bindings.size(), VK_FLAGS_NONE);
    binding_flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    resources.hzb_pipe.desc_set_layout = create_descriptor_set_layout(backend.device, bindings, binding_flags);

    {
        const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                         sizeof(HZBReducePushConstants)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, std::span(&resources.hzb_pipe.desc_set_layout, 1), std::span(&push_constant_range, 1));

        const VkPipelineShaderStageCreateInfo shader_stage = default_pipeline_shader_stage_create_info(
            VK_SHADER_STAGE_COMPUTE_BIT, shader_modules.hzb_reduce_cs, nullptr,
            VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT);

        resources.hzb_pipe.handle = create_compute_pipeline(backend.device, pipeline_layout, shader_stage);
        resources.hzb_pipe.layout = pipeline_layout;
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.hzb_pipe.desc_set_layout, 1),
                             std::span(&resources.descriptor_set, 1));

    return resources;
}

void destroy_hzb_pass_resources(VulkanBackend& backend, const HZBPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.hzb_pipe.handle, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.hzb_pipe.layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.hzb_pipe.desc_set_layout, nullptr);
}

void update_hzb_pass_descriptor_set(DescriptorWriteHelper& write_helper, const HZBPassResources& resources,
                                    const SamplerResources& sampler_resources, const FrameGraphTexture& scene_depth,
                                    const FrameGraphTexture& hzb_texture)
{
    write_helper.append(resources.descriptor_set, Slot_LinearClampSampler, sampler_resources.linear_clamp);
    write_helper.append(resources.descriptor_set, Slot_SceneDepth, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        scene_depth.default_view_handle, scene_depth.image_layout);

    const u32                        hzb_mip_count = hzb_texture.properties.mip_count;
    std::span<VkDescriptorImageInfo> hzb_mips = write_helper.new_image_infos(hzb_mip_count);

    Assert(hzb_texture.additional_views.size() == hzb_mip_count);

    for (u32 index = 0; index < hzb_mips.size(); index += 1)
    {
        hzb_mips[index] = create_descriptor_image_info(hzb_texture.additional_views[index], hzb_texture.image_layout);
    }

    write_helper.writes.push_back(create_image_descriptor_write(resources.descriptor_set, Slot_HZB_mips,
                                                                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, hzb_mips));
}

void record_hzb_command_buffer(CommandBuffer& cmdBuffer, const HZBPassResources& pass_resources,
                               VkExtent2D depth_extent, VkExtent2D hzb_extent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pass_resources.hzb_pipe.handle);

    HZBReducePushConstants push_constants;
    push_constants.depth_extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(depth_extent.width), 1.f / static_cast<float>(depth_extent.height));

    vkCmdPushConstants(cmdBuffer.handle, pass_resources.hzb_pipe.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pass_resources.hzb_pipe.layout, 0, 1,
                            &pass_resources.descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(hzb_extent.width, HZBReduceThreadCountX),
                  div_round_up(hzb_extent.height, HZBReduceThreadCountY),
                  1);
}
} // namespace Reaper
