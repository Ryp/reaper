////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2024 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ToneMappingPass.h"

#include "FrameGraphPass.h"

#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/ShaderModules.h"

#include "profiling/Scope.h"

#include "renderer/shader/tone_mapping_bake_lut.share.hlsl"

namespace Reaper
{
namespace
{
    VkPipeline create_tone_map_bake_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                             VkPipelineLayout pipeline_layout)
    {
        const VkShaderModuleCreateInfo module_create_info =
            shader_module_create_info(get_spirv_shader_module(shader_modules, "tone_mapping_bake_lut.comp.spv"));

        const VkPipelineShaderStageCreateInfo shader_stage =
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, &module_create_info);

        return create_compute_pipeline(device, pipeline_layout, shader_stage);
    }
} // namespace

ToneMapPassResources create_tone_map_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory)
{
    ToneMapPassResources resources = {};

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    resources.descriptor_set_layout = create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

    {
        const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                         sizeof(ToneMappingBakeLUT_Consts)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, std::span(&resources.descriptor_set_layout, 1), std::span(&push_constant_range, 1));

        resources.pipeline_layout = pipeline_layout;
        resources.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_tone_map_bake_pipeline,
                                      });
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.descriptor_set_layout, 1), std::span(&resources.descriptor_set, 1));

    return resources;
}

void destroy_tone_map_pass_resources(VulkanBackend& backend, const ToneMapPassResources& resources)
{
    vkDestroyPipelineLayout(backend.device, resources.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.descriptor_set_layout, nullptr);
}

ToneMapPassRecord create_tone_map_pass_record(FrameGraph::Builder& builder)
{
    ToneMapPassRecord record;
    record.pass_handle = builder.create_render_pass("Tone Map Bake LUT");

    GPUTextureProperties tonemap_lut_properties = default_texture_properties(
        ToneMappingBakeLUT_Res, 1, PixelFormat::R32_SFLOAT, GPUTextureUsage::Storage | GPUTextureUsage::Sampled);
    tonemap_lut_properties.type = GPUTextureType::Tex1D;

    record.tone_map_lut =
        builder.create_texture(record.pass_handle, "Tone Map LUT", tonemap_lut_properties,
                               GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                                                VK_IMAGE_LAYOUT_GENERAL});

    return record;
}

void update_tone_map_pass_descriptor_set(const FrameGraph::FrameGraph& frame_graph,
                                         const FrameGraphResources&    frame_graph_resources,
                                         const ToneMapPassRecord&      record,
                                         DescriptorWriteHelper&        write_helper,
                                         const ToneMapPassResources&   resources)
{
    const FrameGraphTexture tone_map_lut =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.tone_map_lut);

    write_helper.append(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, tone_map_lut.default_view_handle,
                        tone_map_lut.image_layout);
}

void record_tone_map_command_buffer(const FrameGraphHelper& frame_graph_helper, const ToneMapPassRecord& pass_record,
                                    CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                    const ToneMapPassResources& pass_resources, float tonemap_min_nits,
                                    float tonemap_max_nits)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Tone Map Bake LUT");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, pass_resources.pipeline_index));

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pass_resources.pipeline_layout, 0, 1,
                            &pass_resources.descriptor_set, 0, nullptr);

    ToneMappingBakeLUT_Consts push_constants;
    push_constants.min_nits = tonemap_min_nits;
    push_constants.max_nits = tonemap_max_nits;

    vkCmdPushConstants(cmdBuffer.handle, pass_resources.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdDispatch(cmdBuffer.handle, div_round_up(ToneMappingBakeLUT_Res, ToneMappingBakeLUT_ThreadCount), 1, 1);
}
} // namespace Reaper
