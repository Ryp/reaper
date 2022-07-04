////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"
#include <vulkan_loader/Vulkan.h>

#include <glm/vec2.hpp>

#include <nonstd/span.hpp>

#include <vector>

namespace Reaper
{
struct HistogramPipelineInfo
{
    VkPipeline       pipeline;
    VkPipelineLayout pipelineLayout;
};

struct HistogramPassResources
{
    BufferInfo passConstantBuffer;

    VkDescriptorSetLayout descSetLayout;
    HistogramPipelineInfo histogramPipe;

    VkDescriptorSet descriptor_set;
};

struct ReaperRoot;
struct VulkanBackend;
struct FrameGraphBuffer;

HistogramPassResources create_histogram_pass_resources(ReaperRoot& root, VulkanBackend& backend);
void destroy_histogram_pass_resources(VulkanBackend& backend, const HistogramPassResources& resources);

struct SamplerResources;

void update_histogram_pass_descriptor_set(VulkanBackend& backend, const HistogramPassResources& resources,
                                          const SamplerResources& sampler_resources, VkImageView scene_hdr_view,
                                          const FrameGraphBuffer& histogram_buffer);

void upload_histogram_frame_resources(VulkanBackend& backend, const HistogramPassResources& pass_resources,
                                      VkExtent2D backbufferExtent);

struct CommandBuffer;
struct FrameData;

void record_histogram_command_buffer(CommandBuffer& cmdBuffer, const FrameData& frame_data,
                                     const HistogramPassResources& pass_resources);
} // namespace Reaper
