////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/api/Vulkan.h"

#include <glm/vec2.hpp>

#include <nonstd/span.hpp>

#include <vector>

namespace Reaper
{
struct HistogramPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

HistogramPipelineInfo create_histogram_pipeline(ReaperRoot& root, VulkanBackend& backend);

struct HistogramPassResources
{
    BufferInfo passConstantBuffer;
    BufferInfo passHistogramBuffer;

    HistogramPipelineInfo histogramPipe;

    VkDescriptorSet descriptor_set; // FIXME
};

HistogramPassResources create_histogram_pass_resources(ReaperRoot& root, VulkanBackend& backend);
void                   destroy_histogram_pass_resources(VulkanBackend& backend, HistogramPassResources& resources);

VkDescriptorSet create_histogram_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                     const HistogramPassResources& resources, VkImageView texture_view);

struct FrameData;

void record_histogram_command_buffer(VkCommandBuffer cmdBuffer, const FrameData& frame_data,
                                     const HistogramPassResources& pass_resources);
} // namespace Reaper
