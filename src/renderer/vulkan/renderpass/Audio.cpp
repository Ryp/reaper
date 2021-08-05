////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Audio.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Shader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/Profile.h"

#include <array>

#include "renderer/shader/share/sound.hlsl"

namespace Reaper
{
namespace
{
    VkDescriptorSet create_audio_descriptor_sets(ReaperRoot& root, VulkanBackend& backend, AudioResources& resources)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1,
                                                              &resources.audioPipe.descSetLayout};

        VkDescriptorSet audioPassDescriptorSet = VK_NULL_HANDLE;

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &audioPassDescriptorSet)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(audioPassDescriptorSet));

        return audioPassDescriptorSet;
    }

    AudioPipelineInfo create_audio_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

        const VkPushConstantRange audioPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SoundPushConstants)};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         nullptr,
                                                         VK_FLAGS_NONE,
                                                         1,
                                                         &descriptorSetLayout,
                                                         1,
                                                         &audioPushConstantRange};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

        log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkShaderModule        computeShader = VK_NULL_HANDLE;
        const char*           fileName = "./build/shader/oscillator.comp.spv"; // FIXME preserve hierarchy
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

        Assert(vkCreateComputePipelines(backend.device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        log_debug(root, "vulkan: created compute pipeline with handle: {}", static_cast<void*>(pipeline));

        return AudioPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
    }
} // namespace

AudioResources create_audio_resources(ReaperRoot& root, VulkanBackend& backend)
{
    AudioResources resources = {};

    resources.audioPipe = create_audio_pipeline(root, backend);

    resources.audioPassConstantBuffer = create_buffer(
        root, backend.device, "Audio Constant buffer",
        DefaultGPUBufferProperties(1, sizeof(AudioPassParams), GPUBufferUsage::UniformBuffer), backend.vma_instance);

    resources.audioInstanceParamsBuffer = create_buffer(
        root, backend.device, "Audio instance constant buffer",
        DefaultGPUBufferProperties(OscillatorCount, sizeof(OscillatorInstance), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    Assert(SampleSizeInBytes == sizeof(RawSample));

    resources.audioOutputBuffer =
        create_buffer(root, backend.device, "Output sample buffer",
                      DefaultGPUBufferProperties(FrameCountPerGroup * FrameCountPerDispatch, sizeof(RawSample),
                                                 GPUBufferUsage::TransferSrc | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.audioOutputBufferStaging =
        create_buffer(root, backend.device, "Output sample buffer staging",
                      DefaultGPUBufferProperties(FrameCountPerGroup * FrameCountPerDispatch, sizeof(RawSample),
                                                 GPUBufferUsage::TransferDst),
                      backend.vma_instance, MemUsage::GPU_to_CPU);

    VkSemaphoreTypeCreateInfo timelineCreateInfo;
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.pNext = NULL;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = &timelineCreateInfo;
    createInfo.flags = 0;

    vkCreateSemaphore(backend.device, &createInfo, NULL, &resources.semaphore);

    resources.descriptor_set = create_audio_descriptor_sets(root, backend, resources);

    resources.current_frame = 0;

    return resources;
}

void destroy_audio_resources(VulkanBackend& backend, AudioResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.audioPassConstantBuffer.buffer,
                     resources.audioPassConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.audioInstanceParamsBuffer.buffer,
                     resources.audioInstanceParamsBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.audioOutputBuffer.buffer, resources.audioOutputBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.audioOutputBufferStaging.buffer,
                     resources.audioOutputBufferStaging.allocation);

    vkDestroyPipeline(backend.device, resources.audioPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.audioPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.audioPipe.descSetLayout, nullptr);

    vkDestroySemaphore(backend.device, resources.semaphore, nullptr);
}

void upload_audio_frame_resources(VulkanBackend& backend, const PreparedData& prepared, AudioResources& resources)
{
    // FIXME
    // upload_buffer_data(backend.device, backend.vma_instance, resources.audioPassConstantBuffer,
    //                    prepared.audio_pass_params.data(),
    //                    prepared.audio_pass_params.size() * sizeof(AudioPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, resources.audioInstanceParamsBuffer,
                       prepared.audio_instance_params.data(),
                       prepared.audio_instance_params.size() * sizeof(OscillatorInstance));
}

void update_audio_pass_descriptor_set(VulkanBackend& backend, AudioResources& resources)
{
    const VkDescriptorBufferInfo audioDescPassParams =
        default_descriptor_buffer_info(resources.audioPassConstantBuffer);
    const VkDescriptorBufferInfo audioDescInstanceParams =
        default_descriptor_buffer_info(resources.audioInstanceParamsBuffer);
    const VkDescriptorBufferInfo audioDescOutput = default_descriptor_buffer_info(resources.audioOutputBuffer);

    std::array<VkWriteDescriptorSet, 3> audioPassDescriptorSetWrites = {
        create_buffer_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                       &audioDescPassParams),
        create_buffer_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &audioDescInstanceParams),
        create_buffer_descriptor_write(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &audioDescOutput),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(audioPassDescriptorSetWrites.size()),
                           audioPassDescriptorSetWrites.data(), 0, nullptr);
}

void record_audio_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared, AudioResources& resources)
{
    REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Audio Pass", MP_GREEN);

    {
        VkBufferMemoryBarrier buf_barrier = {};
        buf_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buf_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        buf_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        buf_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buf_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buf_barrier.buffer = resources.audioOutputBuffer.buffer;
        buf_barrier.offset = 0;
        buf_barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmdBuffer.handle, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 1, &buf_barrier, 0, nullptr);
    }

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.audioPipe.pipeline);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.audioPipe.pipelineLayout, 0, 1,
                            &resources.descriptor_set, 0, nullptr);

    vkCmdPushConstants(cmdBuffer.handle, resources.audioPipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(SoundPushConstants), &prepared.audio_push_constants);

    vkCmdDispatch(cmdBuffer.handle, FrameCountPerDispatch, 1, 1);

    {
        VkBufferMemoryBarrier buf_barrier = {};
        buf_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buf_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        buf_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        buf_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buf_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buf_barrier.buffer = resources.audioOutputBuffer.buffer;
        buf_barrier.offset = 0;
        buf_barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmdBuffer.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 1, &buf_barrier, 0, nullptr);
    }

    {
        VkBufferMemoryBarrier buf_barrier = {};
        buf_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buf_barrier.srcAccessMask = VK_ACCESS_HOST_READ_BIT;
        buf_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        buf_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buf_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buf_barrier.buffer = resources.audioOutputBufferStaging.buffer;
        buf_barrier.offset = 0;
        buf_barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmdBuffer.handle, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 1, &buf_barrier, 0, nullptr);
    }

    VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = FrameCountPerGroup * FrameCountPerDispatch * sizeof(RawSample);
    vkCmdCopyBuffer(cmdBuffer.handle, resources.audioOutputBuffer.buffer, resources.audioOutputBufferStaging.buffer, 1,
                    &region);

    {
        VkBufferMemoryBarrier buf_barrier = {};
        buf_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buf_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        buf_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        buf_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buf_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buf_barrier.buffer = resources.audioOutputBufferStaging.buffer;
        buf_barrier.offset = 0;
        buf_barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmdBuffer.handle, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                             nullptr, 1, &buf_barrier, 0, nullptr);
    }
}

void read_gpu_audio_data(VulkanBackend& backend, AudioResources& resources)
{
    // u64                 wait_value = 1;
    // VkSemaphoreWaitInfo wait_semaphore = {VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    //                                       nullptr,
    //                                       VK_SEMAPHORE_WAIT_ANY_BIT,
    //                                       1,
    //                                       &resources.semaphore,
    //                                       &wait_value};

    // u64      timeout_us = 100000000u;
    // VkResult result = vkWaitSemaphores(backend.device, &wait_semaphore, timeout_us);
    // Assert(result == VK_SUCCESS);

    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(backend.vma_instance, resources.audioOutputBufferStaging.allocation, &allocation_info);

    const u32 audio_buffer_offset = allocation_info.offset;
    const u32 audio_buffer_size = FrameCountPerGroup * FrameCountPerDispatch * sizeof(RawSample);
    void*     mapped_data_ptr = nullptr;

    Assert(audio_buffer_offset % backend.physicalDeviceProperties.limits.nonCoherentAtomSize == 0);
    Assert(audio_buffer_size % backend.physicalDeviceProperties.limits.nonCoherentAtomSize == 0);
    Assert(vkMapMemory(backend.device, allocation_info.deviceMemory, audio_buffer_offset, audio_buffer_size, 0,
                       &mapped_data_ptr)
           == VK_SUCCESS);

    VkMappedMemoryRange staging_range = {};
    staging_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    staging_range.memory = allocation_info.deviceMemory;
    staging_range.offset = allocation_info.offset;
    staging_range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(backend.device, 1, &staging_range);

    Assert(mapped_data_ptr);
    std::vector<u8> mapped_data_vector(static_cast<u8*>(mapped_data_ptr),
                                       static_cast<u8*>(mapped_data_ptr) + audio_buffer_size);
    resources.frame_audio_data = mapped_data_vector; // Deep copy

    vkUnmapMemory(backend.device, allocation_info.deviceMemory);
}
} // namespace Reaper
