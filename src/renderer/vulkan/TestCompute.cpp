////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestCompute.h"

#include "Shader.h"
#include "Swapchain.h"
#include "SwapchainRendererBase.h"
#include "Test.h"

#include "allocator/GPUStackAllocator.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

namespace Reaper
{
void vulkan_test_compute(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources)
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingTest = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                                                   VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                               nullptr, 0, 1, &descriptorSetLayoutBindingTest};

    VkDescriptorSetLayout descriptorSetLayoutOne = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutOne)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created descriptor set layout with handle: {}",
              static_cast<void*>(descriptorSetLayoutOne));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayoutOne, 0, nullptr};

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

    log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

    // TODO make this a real shader
    VkShaderModule        computeShader = VK_NULL_HANDLE;
    const char*           fileName = "./build/spv/test.comp.spv";
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

    log_debug(root, "vulkan: created pipeline with handle: {}", static_cast<void*>(pipeline));

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                          resources.descriptorPool, 1, &descriptorSetLayoutOne};

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptorSet) == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptorSet));

    VkDescriptorImageInfo descriptorSetImageInfo = {VK_NULL_HANDLE, resources.imageView, VK_IMAGE_LAYOUT_GENERAL};

    VkWriteDescriptorSet descriptorSetWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        descriptorSet,
        0, // 0th binding
        0, // not an array
        1, // not an array
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        &descriptorSetImageInfo,
        nullptr,
        nullptr,
    };

    vkUpdateDescriptorSets(backend.device, 1, &descriptorSetWrite, 0, nullptr);

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        0,      // Not caring yet
        nullptr // No inheritance yet
    };

    Assert(vkBeginCommandBuffer(resources.gfxCmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

    VkImageMemoryBarrier imageBarrierInfo = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        nullptr,
        0, // VK_ACCESS_SHADER_WRITE_BIT ?
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        backend.physicalDeviceInfo.graphicsQueueIndex,
        backend.physicalDeviceInfo.graphicsQueueIndex,
        resources.image.handle,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

    vkCmdPipelineBarrier(resources.gfxCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrierInfo);

    vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                            &descriptorSet, 0, nullptr);
    vkCmdDispatch(resources.gfxCmdBuffer, 1, 1, 1);

    Assert(vkEndCommandBuffer(resources.gfxCmdBuffer) == VK_SUCCESS);

    VkFenceCreateInfo fenceInfo = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
        0 // Not signaled by default
    };

    VkFence drawFence = VK_NULL_HANDLE;
    vkCreateFence(backend.device, &fenceInfo, nullptr, &drawFence);
    log_debug(root, "vulkan: created fence with handle: {}", static_cast<void*>(drawFence));

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0,      nullptr, nullptr, 1,
                               &resources.gfxCmdBuffer,       0,       nullptr};

    log_debug(root, "vulkan: queue submit");
    Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &submitInfo, drawFence) == VK_SUCCESS);

    log_debug(root, "vulkan: wait for fence");
    {
        const u64 waitTimeoutUs = 1000000;
        VkResult  waitResult = vkWaitForFences(backend.device, 1, &drawFence, VK_TRUE, waitTimeoutUs);
        Assert(waitResult == VK_SUCCESS || waitResult == VK_TIMEOUT);
        Assert(waitResult != VK_TIMEOUT); // TODO Handle timeout correctly

        Assert(vkGetFenceStatus(backend.device, drawFence) == VK_SUCCESS);

        Assert(vkResetFences(backend.device, 1, &drawFence) == VK_SUCCESS);
    }

    // cleanup
    vkDestroyFence(backend.device, drawFence, nullptr);

    vkDestroyPipeline(backend.device, pipeline, nullptr);

    vkDestroyShaderModule(backend.device, computeShader, nullptr);
    vkDestroyPipelineLayout(backend.device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, descriptorSetLayoutOne, nullptr);
}
} // namespace Reaper
