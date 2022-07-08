////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "HistogramPass.h"

#include "Frame.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "profiling/Scope.h"

#include "renderer/shader/share/color_space.hlsl"
#include "renderer/shader/share/hdr.hlsl"

namespace Reaper
{
HistogramPassResources create_histogram_pass_resources(ReaperRoot& /*root*/, VulkanBackend& backend,
                                                       const ShaderModules& shader_modules)
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
            backend.device, nonstd::span(&resources.descSetLayout, 1), nonstd::span(&push_constant_range, 1));

        resources.histogramPipe.pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.histogram_cs);
        resources.histogramPipe.pipelineLayout = pipeline_layout;
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, nonstd::span(&resources.descSetLayout, 1),
                             nonstd::span(&resources.descriptor_set, 1));

    return resources;
}

void destroy_histogram_pass_resources(VulkanBackend& backend, const HistogramPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.histogramPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.histogramPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.descSetLayout, nullptr);
}

void update_histogram_pass_descriptor_set(VulkanBackend& backend, const HistogramPassResources& resources,
                                          const SamplerResources& sampler_resources, VkImageView scene_hdr_view,
                                          const FrameGraphBuffer& histogram_buffer)
{
    const VkDescriptorImageInfo  descSampler = {sampler_resources.linearClampSampler, VK_NULL_HANDLE,
                                               VK_IMAGE_LAYOUT_UNDEFINED};
    const VkDescriptorImageInfo  descTexture = {VK_NULL_HANDLE, scene_hdr_view,
                                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorBufferInfo descOutput = {histogram_buffer.handle, histogram_buffer.view.offset_bytes,
                                               histogram_buffer.view.size_bytes};

    std::vector<VkWriteDescriptorSet> drawPassDescriptorSetWrites = {
        create_image_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_SAMPLER, &descSampler),
        create_image_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &descTexture),
        create_buffer_descriptor_write(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descOutput),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(drawPassDescriptorSetWrites.size()),
                           drawPassDescriptorSetWrites.data(), 0, nullptr);
}

void record_histogram_command_buffer(CommandBuffer& cmdBuffer, const FrameData& frame_data,
                                     const HistogramPassResources& pass_resources, VkExtent2D backbufferExtent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pass_resources.histogramPipe.pipeline);

    ReduceHDRPassParams push_constants;
    push_constants.extent_ts = glm::uvec2(backbufferExtent.width, backbufferExtent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(backbufferExtent.width), 1.f / static_cast<float>(backbufferExtent.height));

    vkCmdPushConstants(cmdBuffer.handle, pass_resources.histogramPipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pass_resources.histogramPipe.pipelineLayout, 0, 1, &pass_resources.descriptor_set, 0,
                            nullptr);

    Assert(HistogramRes % (HistogramThreadCountX * HistogramThreadCountY) == 0);
    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(frame_data.backbufferExtent.width, HistogramThreadCountX * 2),
                  div_round_up(frame_data.backbufferExtent.height, HistogramThreadCountY * 2),
                  1);
}
} // namespace Reaper
