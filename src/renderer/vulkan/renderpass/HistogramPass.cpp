////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "HistogramPass.h"

#include "Frame.h"

#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/SwapchainRendererBase.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/Profile.h"

#include "renderer/shader/share/color_space.hlsl"
#include "renderer/shader/share/hdr.hlsl"

namespace Reaper
{
namespace
{
    VkDescriptorSet create_histogram_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                         VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1, &layout};

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

        return descriptor_set;
    }
} // namespace
HistogramPipelineInfo create_histogram_pipeline(ReaperRoot& root, VulkanBackend& backend)
{
    std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetLayoutBinding = {
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
        static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created descriptor set layout with handle: {}", static_cast<void*>(descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayout, 0, nullptr};

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

    log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

    VkShaderModule        computeShader = VK_NULL_HANDLE;
    const char*           fileName = "./build/shader/reduce_hdr_frame.comp.spv";
    const char*           entryPoint = "main";
    VkSpecializationInfo* specialization = nullptr;

    vulkan_create_shader_module(computeShader, backend.device, fileName);

    VkPipelineShaderStageCreateInfo shaderStage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                   nullptr,
                                                   0,
                                                   VK_SHADER_STAGE_COMPUTE_BIT,
                                                   computeShader,
                                                   entryPoint,
                                                   specialization};

    VkComputePipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                      nullptr,
                                                      0,
                                                      shaderStage,
                                                      pipelineLayout,
                                                      VK_NULL_HANDLE, // do not care about pipeline derivatives
                                                      0};

    VkPipeline      pipeline = VK_NULL_HANDLE;
    VkPipelineCache cache = VK_NULL_HANDLE;

    Assert(vkCreateComputePipelines(backend.device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline) == VK_SUCCESS);

    vkDestroyShaderModule(backend.device, computeShader, nullptr);

    log_debug(root, "vulkan: created compute pipeline with handle: {}", static_cast<void*>(pipeline));

    return HistogramPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
}

HistogramPassResources create_histogram_pass_resources(ReaperRoot& root, VulkanBackend& backend)
{
    HistogramPassResources resources = {};

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "Histogram Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(ReduceHDRPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance);

    resources.passHistogramBuffer = create_buffer(
        root, backend.device, "Histogram buffer",
        DefaultGPUBufferProperties(HISTOGRAM_RES, sizeof(u32), GPUBufferUsage::StorageBuffer), backend.vma_instance);

    resources.histogramPipe = create_histogram_pipeline(root, backend);

    resources.descriptor_set =
        create_histogram_pass_descriptor_set(root, backend, resources.histogramPipe.descSetLayout);

    return resources;
}

void destroy_histogram_pass_resources(VulkanBackend& backend, const HistogramPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.histogramPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.histogramPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.histogramPipe.descSetLayout, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.buffer,
                     resources.passConstantBuffer.allocation);

    vmaDestroyBuffer(backend.vma_instance, resources.passHistogramBuffer.buffer,
                     resources.passHistogramBuffer.allocation);
}

void update_histogram_pass_descriptor_set(VulkanBackend& backend, const HistogramPassResources& resources,
                                          VkImageView texture_view)
{
    const VkDescriptorBufferInfo drawDescPassParams = default_descriptor_buffer_info(resources.passConstantBuffer);
    const VkDescriptorImageInfo  descTexture = {VK_NULL_HANDLE, texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorBufferInfo descOutput = default_descriptor_buffer_info(resources.passHistogramBuffer);

    std::array<VkWriteDescriptorSet, 3> drawPassDescriptorSetWrites = {
        create_buffer_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                       &drawDescPassParams),
        create_image_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &descTexture),
        create_buffer_descriptor_write(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descOutput),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(drawPassDescriptorSetWrites.size()),
                           drawPassDescriptorSetWrites.data(), 0, nullptr);
}

void upload_histogram_frame_resources(VulkanBackend& backend, const HistogramPassResources& pass_resources,
                                      VkExtent2D backbufferExtent)
{
    ReduceHDRPassParams params = {};
    params.input_size_ts = glm::uvec2(backbufferExtent.width, backbufferExtent.height);

    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.passConstantBuffer, &params,
                       sizeof(ReduceHDRPassParams));

    std::array<u32, HISTOGRAM_RES> zero = {};
    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.passHistogramBuffer, zero.data(),
                       zero.size() * sizeof(u32));
}

void record_histogram_command_buffer(VkCommandBuffer cmdBuffer, const FrameData& frame_data,
                                     const HistogramPassResources& pass_resources)
{
    REAPER_PROFILE_SCOPE_GPU("Histogram Pass", MP_DARKGOLDENROD);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass_resources.histogramPipe.pipeline);

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass_resources.histogramPipe.pipelineLayout, 0,
                            1, &pass_resources.descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer,
                  div_round_up(frame_data.backbufferExtent.width, ReduceHDRGroupSizeX),
                  div_round_up(frame_data.backbufferExtent.height, ReduceHDRGroupSizeY),
                  1);
}
} // namespace Reaper
