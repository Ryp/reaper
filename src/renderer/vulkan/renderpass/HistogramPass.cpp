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
HistogramPassResources create_histogram_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                       const ShaderModules& shader_modules)
{
    HistogramPassResources resources = {};

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "Histogram Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(ReduceHDRPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    resources.descSetLayout = create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

    {
        VkPipelineLayout pipelineLayout =
            create_pipeline_layout(backend.device, nonstd::span(&resources.descSetLayout, 1));

        resources.histogramPipe.pipeline =
            create_compute_pipeline(backend.device, pipelineLayout, shader_modules.histogram_cs);
        resources.histogramPipe.pipelineLayout = pipelineLayout;
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

    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.handle,
                     resources.passConstantBuffer.allocation);
}

void update_histogram_pass_descriptor_set(VulkanBackend& backend, const HistogramPassResources& resources,
                                          const SamplerResources& sampler_resources, VkImageView scene_hdr_view,
                                          const FrameGraphBuffer& histogram_buffer)
{
    const VkDescriptorBufferInfo drawDescPassParams = default_descriptor_buffer_info(resources.passConstantBuffer);
    const VkDescriptorImageInfo  descSampler = {sampler_resources.linearClampSampler, VK_NULL_HANDLE,
                                               VK_IMAGE_LAYOUT_UNDEFINED};
    const VkDescriptorImageInfo  descTexture = {VK_NULL_HANDLE, scene_hdr_view,
                                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorBufferInfo descOutput = {histogram_buffer.handle, histogram_buffer.view.offset_bytes,
                                               histogram_buffer.view.size_bytes};

    std::vector<VkWriteDescriptorSet> drawPassDescriptorSetWrites = {
        create_buffer_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                       &drawDescPassParams),
        create_image_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLER, &descSampler),
        create_image_descriptor_write(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &descTexture),
        create_buffer_descriptor_write(resources.descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descOutput),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(drawPassDescriptorSetWrites.size()),
                           drawPassDescriptorSetWrites.data(), 0, nullptr);
}

void upload_histogram_frame_resources(VulkanBackend& backend, const HistogramPassResources& pass_resources,
                                      VkExtent2D backbufferExtent)
{
    REAPER_PROFILE_SCOPE_FUNC();

    ReduceHDRPassParams params = {};
    params.extent_ts = glm::uvec2(backbufferExtent.width, backbufferExtent.height);
    params.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(backbufferExtent.width), 1.f / static_cast<float>(backbufferExtent.height));

    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.passConstantBuffer, &params,
                       sizeof(ReduceHDRPassParams));
}

void record_histogram_command_buffer(CommandBuffer& cmdBuffer, const FrameData& frame_data,
                                     const HistogramPassResources& pass_resources)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pass_resources.histogramPipe.pipeline);

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
