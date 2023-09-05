////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/vulkan/Buffer.h"
#include <vulkan_loader/Vulkan.h>

#include <vector>

namespace Reaper
{
struct VulkanBackend;
struct ShaderModules;

struct AudioFrameGraphData
{
    struct Render
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle audio_buffer;
    } render;

    struct StagingCopy
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle audio_buffer;
    } staging_copy;
};

struct AudioPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct AudioResources
{
    AudioPipelineInfo audioPipe;

    GPUBuffer           instance_buffer;
    GPUBuffer           audio_staging_buffer;
    GPUBufferProperties audio_staging_properties;

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

namespace FrameGraph
{
    class Builder;
}

AudioFrameGraphData create_audio_frame_graph_data(FrameGraph::Builder& builder);

AudioResources create_audio_resources(VulkanBackend& backend, const ShaderModules& shader_modules);
void           destroy_audio_resources(VulkanBackend& backend, AudioResources& resources);

struct PreparedData;

void upload_audio_frame_resources(VulkanBackend& backend, const PreparedData& prepared, AudioResources& resources);

class DescriptorWriteHelper;
struct FrameGraphBuffer;

void update_audio_render_descriptor_set(DescriptorWriteHelper& write_helper, AudioResources& resources,
                                        const FrameGraphBuffer& audio_buffer);

struct CommandBuffer;

void record_audio_render_command_buffer(CommandBuffer&      cmdBuffer,
                                        const PreparedData& prepared,
                                        AudioResources&     resources);

void record_audio_copy_command_buffer(CommandBuffer& cmdBuffer, AudioResources& resources,
                                      const FrameGraphBuffer& audio_buffer);

void read_gpu_audio_data(VulkanBackend& backend, AudioResources& resources);
} // namespace Reaper
