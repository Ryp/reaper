////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "HistogramPass.h"

#include "renderer/graph/FrameGraphBuilder.h"
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

#include "renderer/shader/histogram/reduce_histogram.share.hlsl"

namespace Reaper
{
HistogramPassResources create_histogram_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules)
{
    HistogramPassResources resources = {};

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    resources.descSetLayout = create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

    {
        const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ReduceHDRPassParams)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, std::span(&resources.descSetLayout, 1), std::span(&push_constant_range, 1));

        resources.histogramPipe.pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.histogram_cs);
        resources.histogramPipe.pipelineLayout = pipeline_layout;
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, std::span(&resources.descSetLayout, 1),
                             std::span(&resources.descriptor_set, 1));

    return resources;
}

void destroy_histogram_pass_resources(VulkanBackend& backend, const HistogramPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.histogramPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.histogramPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.descSetLayout, nullptr);
}

HistogramClearFrameGraphRecord create_histogram_clear_pass_record(FrameGraph::Builder& builder)
{
    HistogramClearFrameGraphRecord histogram_clear;
    histogram_clear.pass_handle = builder.create_render_pass("Histogram Clear");

    const GPUBufferProperties histogram_buffer_properties = DefaultGPUBufferProperties(
        HistogramRes, sizeof(u32), GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    histogram_clear.histogram_buffer =
        builder.create_buffer(histogram_clear.pass_handle, "Histogram Buffer", histogram_buffer_properties,
                              GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    return histogram_clear;
}

HistogramFrameGraphRecord create_histogram_pass_record(FrameGraph::Builder&                  builder,
                                                       const HistogramClearFrameGraphRecord& histogram_clear,
                                                       FrameGraph::ResourceUsageHandle       scene_hdr_usage_handle)
{
    HistogramFrameGraphRecord histogram;
    histogram.pass_handle = builder.create_render_pass("Histogram");

    histogram.scene_hdr =
        builder.read_texture(histogram.pass_handle, scene_hdr_usage_handle,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    histogram.histogram_buffer =
        builder.write_buffer(histogram.pass_handle, histogram_clear.histogram_buffer,
                             GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT});

    return histogram;
}

void update_histogram_pass_descriptor_set(const FrameGraph::FrameGraph&    frame_graph,
                                          const FrameGraphResources&       frame_graph_resources,
                                          const HistogramFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                          const HistogramPassResources& resources,
                                          const SamplerResources&       sampler_resources)
{
    const FrameGraphTexture scene_hdr = get_frame_graph_texture(frame_graph_resources, frame_graph, record.scene_hdr);
    const FrameGraphBuffer  histogram_buffer =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.histogram_buffer);

    write_helper.append(resources.descriptor_set, 0, sampler_resources.linear_clamp);
    write_helper.append(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, scene_hdr.default_view_handle,
                        scene_hdr.image_layout);
    write_helper.append(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, histogram_buffer.handle);
}

void record_histogram_command_buffer(CommandBuffer& cmdBuffer, const HistogramPassResources& pass_resources,
                                     VkExtent2D render_extent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pass_resources.histogramPipe.pipeline);

    ReduceHDRPassParams push_constants;
    push_constants.extent_ts = glm::uvec2(render_extent.width, render_extent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(render_extent.width), 1.f / static_cast<float>(render_extent.height));

    vkCmdPushConstants(cmdBuffer.handle, pass_resources.histogramPipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pass_resources.histogramPipe.pipelineLayout, 0, 1, &pass_resources.descriptor_set, 0,
                            nullptr);

    Assert(HistogramRes % (HistogramThreadCountX * HistogramThreadCountY) == 0);
    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(render_extent.width, HistogramThreadCountX * 2),
                  div_round_up(render_extent.height, HistogramThreadCountY * 2),
                  1);
}
} // namespace Reaper
