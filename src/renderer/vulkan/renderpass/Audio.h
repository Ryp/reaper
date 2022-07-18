////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include <vulkan_loader/Vulkan.h>

#include <vector>

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

struct AudioPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct AudioResources
{
    AudioPipelineInfo audioPipe;

    BufferInfo passConstantBuffer;
    BufferInfo instanceParamsBuffer;
    BufferInfo outputBuffer;
    BufferInfo outputBufferStaging;

    VkDescriptorSet descriptor_set;

    VkSemaphore semaphore;

    u32             current_frame; // FIXME a few hours of audio can be indexed with a u32
    std::vector<u8> frame_audio_data;
};

#if 1
struct RawSample
{
    u32 l; // encoded as float
    u32 r; // encoded as float
};
#else
struct RawSample
{
    u16 l; // encoded as float
    u16 r; // encoded as float
};
#endif

AudioResources create_audio_resources(ReaperRoot& root, VulkanBackend& backend, const ShaderModules& shader_modules);
void           destroy_audio_resources(VulkanBackend& backend, AudioResources& resources);

struct PreparedData;

void upload_audio_frame_resources(VulkanBackend& backend, const PreparedData& prepared, AudioResources& resources);

struct DescriptorWriteHelper;

void update_audio_pass_descriptor_set(DescriptorWriteHelper& write_helper, AudioResources& resources);

struct CommandBuffer;

void record_audio_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared, AudioResources& resources);

void read_gpu_audio_data(VulkanBackend& backend, AudioResources& resources);
} // namespace Reaper
