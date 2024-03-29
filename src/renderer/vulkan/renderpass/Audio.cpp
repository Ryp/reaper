////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Audio.h"

#include "AudioConstants.h"
#include "FrameGraphPass.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/api/AssertHelper.h"

#include "renderer/graph/FrameGraphBuilder.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "profiling/Scope.h"

#include <array>

#include "renderer/shader/sound/sound.share.hlsl"

namespace Reaper
{
namespace
{
    VkPipeline create_audio_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                     VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.oscillator_cs);
    }
} // namespace

AudioResources create_audio_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory)
{
    AudioResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange audioPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SoundPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptorSetLayout, 1),
                                                                 std::span(&audioPushConstantRange, 1));

        resources.descSetLayout = descriptorSetLayout;
        resources.pipelineLayout = pipelineLayout;
        resources.pipeline_index = register_pipeline_creator(pipeline_factory,
                                                             PipelineCreator{
                                                                 .pipeline_layout = pipelineLayout,
                                                                 .pipeline_creation_function = &create_audio_pipeline,
                                                             });
    }

    resources.instance_buffer = create_buffer(
        backend.device, "Audio instance constant buffer",
        DefaultGPUBufferProperties(OscillatorCount, sizeof(OscillatorInstance), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.audio_staging_properties = DefaultGPUBufferProperties(FrameCountPerGroup * FrameCountPerDispatch,
                                                                    sizeof(RawSample), GPUBufferUsage::TransferDst);

    resources.audio_staging_buffer =
        create_buffer(backend.device, "Output sample buffer staging", resources.audio_staging_properties,
                      backend.vma_instance, MemUsage::GPU_To_CPU);

    Assert(SampleSizeInBytes == sizeof(RawSample));

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

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, std::span(&resources.descSetLayout, 1),
                             std::span(&resources.descriptor_set, 1));

    resources.current_frame = 0;

    return resources;
}

void destroy_audio_resources(VulkanBackend& backend, AudioResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.instance_buffer.handle, resources.instance_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.audio_staging_buffer.handle,
                     resources.audio_staging_buffer.allocation);

    vkDestroyPipelineLayout(backend.device, resources.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.descSetLayout, nullptr);

    vkDestroySemaphore(backend.device, resources.semaphore, nullptr);
}

AudioFrameGraphRecord create_audio_frame_graph_record(FrameGraph::Builder& builder)
{
    AudioFrameGraphRecord::Render render;

    render.pass_handle = builder.create_render_pass("Audio render");

    render.audio_buffer =
        builder.create_buffer(render.pass_handle, "Output sample buffer",
                              DefaultGPUBufferProperties(FrameCountPerGroup * FrameCountPerDispatch, sizeof(RawSample),
                                                         GPUBufferUsage::TransferSrc | GPUBufferUsage::StorageBuffer),
                              GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    AudioFrameGraphRecord::StagingCopy staging_copy;

    staging_copy.pass_handle = builder.create_render_pass("Audio staging copy", true);

    staging_copy.audio_buffer =
        builder.read_buffer(staging_copy.pass_handle, render.audio_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT});

    return {
        .render = render,
        .staging_copy = staging_copy,
    };
}

void update_audio_render_resources(VulkanBackend& backend, const FrameGraph::FrameGraph& frame_graph,
                                   const FrameGraphResources&   frame_graph_resources,
                                   const AudioFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                   const PreparedData& prepared, const AudioResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.instance_buffer,
                                  prepared.audio_instance_params.data(),
                                  prepared.audio_instance_params.size() * sizeof(OscillatorInstance));

    const FrameGraphBuffer audio_buffer =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.render.audio_buffer);

    write_helper.append(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.instance_buffer.handle);
    write_helper.append(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, audio_buffer.handle);
}

void record_audio_render_command_buffer(const FrameGraphHelper&              frame_graph_helper,
                                        const AudioFrameGraphRecord::Render& pass_record, CommandBuffer& cmdBuffer,
                                        const PipelineFactory& pipeline_factory, const PreparedData& prepared,
                                        AudioResources& resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Audio Render");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, resources.pipeline_index));

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.pipelineLayout, 0, 1,
                            &resources.descriptor_set, 0, nullptr);

    vkCmdPushConstants(cmdBuffer.handle, resources.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(SoundPushConstants), &prepared.audio_push_constants);

    vkCmdDispatch(cmdBuffer.handle, FrameCountPerDispatch, 1, 1);

    {
        const GPUBufferAccess src = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
        const GPUBufferAccess dst = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        const GPUBufferView   view = default_buffer_view(resources.audio_staging_properties);

        const VkBufferMemoryBarrier2 buffer_barrier =
            get_vk_buffer_barrier(resources.audio_staging_buffer.handle, view, src, dst);

        const VkDependencyInfo dependencies = get_vk_buffer_barrier_depency_info(std::span(&buffer_barrier, 1));

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }
}

void record_audio_copy_command_buffer(const FrameGraphHelper&                   frame_graph_helper,
                                      const AudioFrameGraphRecord::StagingCopy& pass_record,
                                      CommandBuffer&                            cmdBuffer,
                                      AudioResources&                           resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Audio Staging Copy");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const FrameGraphBuffer audio_buffer =
        get_frame_graph_buffer(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.audio_buffer);

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
        .srcBuffer = audio_buffer.handle,
        .dstBuffer = resources.audio_staging_buffer.handle,
        .regionCount = 1,
        .pRegions = &region,
    };

    vkCmdCopyBuffer2(cmdBuffer.handle, &copy);

    {
        const GPUBufferAccess src = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        const GPUBufferAccess dst = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
        const GPUBufferView   view = default_buffer_view(resources.audio_staging_properties);

        const VkBufferMemoryBarrier2 buffer_barrier =
            get_vk_buffer_barrier(resources.audio_staging_buffer.handle, view, src, dst);

        const VkDependencyInfo dependencies = get_vk_buffer_barrier_depency_info(std::span(&buffer_barrier, 1));

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
    vmaGetAllocationInfo(backend.vma_instance, resources.audio_staging_buffer.allocation, &allocation_info);

    const u32 audio_buffer_offset = static_cast<u32>(allocation_info.offset);
    const u32 audio_buffer_size = FrameCountPerGroup * FrameCountPerDispatch * sizeof(RawSample);
    void*     mapped_data_ptr = nullptr;

    Assert(audio_buffer_offset % backend.physical_device.properties.limits.nonCoherentAtomSize == 0);
    Assert(audio_buffer_size % backend.physical_device.properties.limits.nonCoherentAtomSize == 0);
    AssertVk(vkMapMemory(backend.device, allocation_info.deviceMemory, audio_buffer_offset, audio_buffer_size, 0,
                         &mapped_data_ptr));

    const VkMappedMemoryRange staging_range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .pNext = nullptr,
        .memory = allocation_info.deviceMemory,
        .offset = allocation_info.offset,
        .size = allocation_info.size,
    };

    vkInvalidateMappedMemoryRanges(backend.device, 1, &staging_range);

    Assert(mapped_data_ptr);
    std::vector<u8> mapped_data_vector(static_cast<u8*>(mapped_data_ptr),
                                       static_cast<u8*>(mapped_data_ptr) + audio_buffer_size);
    resources.frame_audio_data = mapped_data_vector; // Deep copy

    vkUnmapMemory(backend.device, allocation_info.deviceMemory);
}
} // namespace Reaper
