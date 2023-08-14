////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Audio.h"

#include "AudioConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/ShaderModules.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "profiling/Scope.h"

#include <array>

#include "renderer/shader/sound/sound.share.hlsl"

namespace Reaper
{
AudioResources create_audio_resources(ReaperRoot& root, VulkanBackend& backend, const ShaderModules& shader_modules)
{
    AudioResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange audioPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SoundPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&audioPushConstantRange, 1));

        VkPipeline pipeline = create_compute_pipeline(backend.device, pipelineLayout, shader_modules.oscillator_cs);

        resources.audioPipe = AudioPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
    }

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "Audio Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(AudioPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.instanceParamsBuffer = create_buffer(
        root, backend.device, "Audio instance constant buffer",
        DefaultGPUBufferProperties(OscillatorCount, sizeof(OscillatorInstance), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    Assert(SampleSizeInBytes == sizeof(RawSample));

    resources.outputBuffer =
        create_buffer(root, backend.device, "Output sample buffer",
                      DefaultGPUBufferProperties(FrameCountPerGroup * FrameCountPerDispatch, sizeof(RawSample),
                                                 GPUBufferUsage::TransferSrc | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.outputBufferStaging =
        create_buffer(root, backend.device, "Output sample buffer staging",
                      DefaultGPUBufferProperties(FrameCountPerGroup * FrameCountPerDispatch, sizeof(RawSample),
                                                 GPUBufferUsage::TransferDst),
                      backend.vma_instance, MemUsage::GPU_To_CPU);

    const VkSemaphoreTypeCreateInfo timelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = NULL,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };

    const VkSemaphoreCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineCreateInfo,
        .flags = VK_FLAGS_NONE,
    };

    vkCreateSemaphore(backend.device, &createInfo, NULL, &resources.semaphore);

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             nonstd::span(&resources.audioPipe.descSetLayout, 1),
                             nonstd::span(&resources.descriptor_set, 1));

    resources.current_frame = 0;

    return resources;
}

void destroy_audio_resources(VulkanBackend& backend, AudioResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.handle,
                     resources.passConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.instanceParamsBuffer.handle,
                     resources.instanceParamsBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.outputBuffer.handle, resources.outputBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.outputBufferStaging.handle,
                     resources.outputBufferStaging.allocation);

    vkDestroyPipeline(backend.device, resources.audioPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.audioPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.audioPipe.descSetLayout, nullptr);

    vkDestroySemaphore(backend.device, resources.semaphore, nullptr);
}

void upload_audio_frame_resources(VulkanBackend& backend, const PreparedData& prepared, AudioResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    // FIXME
    // upload_buffer_data(backend.device, backend.vma_instance, resources.passConstantBuffer,
    //                    prepared.audio_pass_params.data(),
    //                    prepared.audio_pass_params.size() * sizeof(AudioPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, resources.instanceParamsBuffer,
                       prepared.audio_instance_params.data(),
                       prepared.audio_instance_params.size() * sizeof(OscillatorInstance));
}

void update_audio_pass_descriptor_set(DescriptorWriteHelper& write_helper, AudioResources& resources)
{
    write_helper.append(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        resources.passConstantBuffer.handle);
    write_helper.append(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.instanceParamsBuffer.handle);
    write_helper.append(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, resources.outputBuffer.handle);
}

void record_audio_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared, AudioResources& resources)
{
    {
        const GPUResourceAccess src = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
        const GPUResourceAccess dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUBufferView     view = default_buffer_view(resources.outputBuffer.properties);

        const VkBufferMemoryBarrier2 buffer_barrier =
            get_vk_buffer_barrier(resources.outputBuffer.handle, view, src, dst);

        const VkDependencyInfo dependencies = get_vk_buffer_barrier_depency_info(1, &buffer_barrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.audioPipe.pipeline);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.audioPipe.pipelineLayout, 0, 1,
                            &resources.descriptor_set, 0, nullptr);

    vkCmdPushConstants(cmdBuffer.handle, resources.audioPipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(SoundPushConstants), &prepared.audio_push_constants);

    vkCmdDispatch(cmdBuffer.handle, FrameCountPerDispatch, 1, 1);

    {
        std::vector<VkBufferMemoryBarrier2> buffer_barriers;

        {
            const GPUResourceAccess src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
            const GPUResourceAccess dst = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
            const GPUBufferView     view = default_buffer_view(resources.outputBuffer.properties);

            buffer_barriers.emplace_back(get_vk_buffer_barrier(resources.outputBuffer.handle, view, src, dst));
        }

        {
            const GPUResourceAccess src = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
            const GPUResourceAccess dst = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
            const GPUBufferView     view = default_buffer_view(resources.outputBufferStaging.properties);

            buffer_barriers.emplace_back(get_vk_buffer_barrier(resources.outputBufferStaging.handle, view, src, dst));
        }

        const VkDependencyInfo dependencies =
            get_vk_buffer_barrier_depency_info(static_cast<uint32_t>(buffer_barriers.size()), buffer_barriers.data());

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    const VkBufferCopy2 region = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .pNext = nullptr,
        .srcOffset = 0,
        .dstOffset = 0,
        .size = FrameCountPerGroup * FrameCountPerDispatch * sizeof(RawSample),
    };

    const VkCopyBufferInfo2 copy = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .pNext = nullptr,
        .srcBuffer = resources.outputBuffer.handle,
        .dstBuffer = resources.outputBufferStaging.handle,
        .regionCount = 1,
        .pRegions = &region,
    };

    vkCmdCopyBuffer2(cmdBuffer.handle, &copy);

    {
        const GPUResourceAccess src = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        const GPUResourceAccess dst = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
        const GPUBufferView     view = default_buffer_view(resources.outputBufferStaging.properties);

        const VkBufferMemoryBarrier2 buffer_barrier =
            get_vk_buffer_barrier(resources.outputBufferStaging.handle, view, src, dst);

        const VkDependencyInfo dependencies = get_vk_buffer_barrier_depency_info(1, &buffer_barrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
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
    vmaGetAllocationInfo(backend.vma_instance, resources.outputBufferStaging.allocation, &allocation_info);

    const u32 audio_buffer_offset = allocation_info.offset;
    const u32 audio_buffer_size = FrameCountPerGroup * FrameCountPerDispatch * sizeof(RawSample);
    void*     mapped_data_ptr = nullptr;

    Assert(audio_buffer_offset % backend.physicalDeviceProperties.limits.nonCoherentAtomSize == 0);
    Assert(audio_buffer_size % backend.physicalDeviceProperties.limits.nonCoherentAtomSize == 0);
    Assert(vkMapMemory(backend.device, allocation_info.deviceMemory, audio_buffer_offset, audio_buffer_size, 0,
                       &mapped_data_ptr)
           == VK_SUCCESS);

    const VkMappedMemoryRange staging_range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .pNext = nullptr,
        .memory = allocation_info.deviceMemory,
        .offset = allocation_info.offset,
        .size = VK_WHOLE_SIZE,
    };

    vkInvalidateMappedMemoryRanges(backend.device, 1, &staging_range);

    Assert(mapped_data_ptr);
    std::vector<u8> mapped_data_vector(static_cast<u8*>(mapped_data_ptr),
                                       static_cast<u8*>(mapped_data_ptr) + audio_buffer_size);
    resources.frame_audio_data = mapped_data_vector; // Deep copy

    vkUnmapMemory(backend.device, allocation_info.deviceMemory);
}
} // namespace Reaper
