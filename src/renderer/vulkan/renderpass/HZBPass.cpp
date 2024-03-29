////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "HZBPass.h"

#include "FrameGraphPass.h"
#include "TiledLightingCommon.h"

#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "profiling/Scope.h"

#include "renderer/shader/hzb_reduce.share.hlsl"

namespace Reaper
{
namespace
{
    enum BindingIndex
    {
        LinearClampSampler,
        SceneDepth,
        HZB_mips,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{
            .slot = 0, .count = 1, .type = VK_DESCRIPTOR_TYPE_SAMPLER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 1, .count = 1, .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 2,
         .count = HZBMaxMipCount,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
    };

    VkPipeline create_hzb_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                   VkPipelineLayout pipeline_layout)
    {
        const VkPipelineShaderStageCreateInfo shader_stage = default_pipeline_shader_stage_create_info(
            VK_SHADER_STAGE_COMPUTE_BIT, shader_modules.hzb_reduce_cs, nullptr,
            VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT);

        return create_compute_pipeline(device, pipeline_layout, shader_stage);
    }
} // namespace

HZBPassResources create_hzb_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory)
{
    HZBPassResources resources = {};

    std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
    fill_layout_bindings(layout_bindings, g_bindings);

    std::vector<VkDescriptorBindingFlags> binding_flags(layout_bindings.size(), VK_FLAGS_NONE);
    binding_flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    resources.hzb_pipe.desc_set_layout = create_descriptor_set_layout(backend.device, layout_bindings, binding_flags);

    const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(HZBReducePushConstants)};

    VkPipelineLayout pipeline_layout = create_pipeline_layout(
        backend.device, std::span(&resources.hzb_pipe.desc_set_layout, 1), std::span(&push_constant_range, 1));

    resources.hzb_pipe.layout = pipeline_layout;
    resources.hzb_pipe.pipeline_index =
        register_pipeline_creator(pipeline_factory,
                                  PipelineCreator{
                                      .pipeline_layout = pipeline_layout,
                                      .pipeline_creation_function = &create_hzb_pipeline,
                                  });

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.hzb_pipe.desc_set_layout, 1),
                             std::span(&resources.descriptor_set, 1));

    return resources;
}

void destroy_hzb_pass_resources(VulkanBackend& backend, const HZBPassResources& resources)
{
    vkDestroyPipelineLayout(backend.device, resources.hzb_pipe.layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.hzb_pipe.desc_set_layout, nullptr);
}

HZBReduceFrameGraphRecord create_hzb_pass_record(FrameGraph::Builder&            builder,
                                                 FrameGraph::ResourceUsageHandle depth_buffer_usage_handle,
                                                 const TiledLightingFrame&       tiled_lighting_frame)
{
    HZBReduceFrameGraphRecord record;

    record.pass_handle = builder.create_render_pass("HZB Reduce");

    record.depth =
        builder.read_texture(record.pass_handle, depth_buffer_usage_handle,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL});

    // NOTE: HZB size is rounded to match the depth used in the tiled lighting raster pass.
    GPUTextureProperties hzb_properties =
        default_texture_properties(tiled_lighting_frame.tile_count_x * 8, tiled_lighting_frame.tile_count_y * 8,
                                   PixelFormat::R16G16_UNORM, GPUTextureUsage::Storage | GPUTextureUsage::Sampled);
    hzb_properties.mip_count = 4; // FIXME

    std::vector<GPUTextureView> hzb_mip_views(hzb_properties.mip_count, default_texture_view(hzb_properties));

    for (u32 i = 0; i < hzb_mip_views.size(); i++)
    {
        hzb_mip_views[i].subresource.mip_count = 1;
        hzb_mip_views[i].subresource.mip_offset = i;
    }

    record.hzb_texture = builder.create_texture(
        record.pass_handle, "HZB Texture", hzb_properties,
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
        hzb_mip_views);

    record.hzb_properties = hzb_properties;

    return record;
}

void update_hzb_pass_descriptor_set(const FrameGraph::FrameGraph&    frame_graph,
                                    const FrameGraphResources&       frame_graph_resources,
                                    const HZBReduceFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                    const HZBPassResources& resources, const SamplerResources& sampler_resources)
{
    const FrameGraphTexture scene_depth = get_frame_graph_texture(frame_graph_resources, frame_graph, record.depth);
    const FrameGraphTexture hzb_texture =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.hzb_texture);

    write_helper.append(resources.descriptor_set, g_bindings[LinearClampSampler], sampler_resources.linear_clamp);
    write_helper.append(resources.descriptor_set, g_bindings[SceneDepth], scene_depth.default_view_handle,
                        scene_depth.image_layout);

    const u32                        hzb_mip_count = hzb_texture.properties.mip_count;
    std::span<VkDescriptorImageInfo> hzb_mips = write_helper.new_image_infos(hzb_mip_count);

    Assert(hzb_texture.additional_views.size() == hzb_mip_count);

    for (u32 index = 0; index < hzb_mips.size(); index += 1)
    {
        hzb_mips[index] = create_descriptor_image_info(hzb_texture.additional_views[index], hzb_texture.image_layout);
    }

    write_helper.writes.push_back(create_image_descriptor_write(resources.descriptor_set, g_bindings[HZB_mips].slot,
                                                                g_bindings[HZB_mips].type, hzb_mips));
}

void record_hzb_command_buffer(const FrameGraphHelper& frame_graph_helper, const HZBReduceFrameGraphRecord& pass_record,
                               CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                               const HZBPassResources& pass_resources, VkExtent2D depth_extent, VkExtent2D hzb_extent)
{
    REAPER_GPU_SCOPE(cmdBuffer, "HZB Reduce");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, pass_resources.hzb_pipe.pipeline_index));

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
