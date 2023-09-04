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

#include <span>

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
    VkDescriptorSetLayout descSetLayout;
    HistogramPipelineInfo histogramPipe;

    VkDescriptorSet descriptor_set;
};

struct ReaperRoot;
struct VulkanBackend;
struct FrameGraphBuffer;
struct ShaderModules;

HistogramPassResources create_histogram_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                       const ShaderModules& shader_modules);
void destroy_histogram_pass_resources(VulkanBackend& backend, const HistogramPassResources& resources);

struct SamplerResources;
class DescriptorWriteHelper;
struct FrameGraphTexture;

void update_histogram_pass_descriptor_set(DescriptorWriteHelper& write_helper, const HistogramPassResources& resources,
                                          const SamplerResources& sampler_resources, const FrameGraphTexture& scene_hdr,
                                          const FrameGraphBuffer& histogram_buffer);

struct CommandBuffer;

void record_histogram_command_buffer(CommandBuffer& cmdBuffer, const HistogramPassResources& pass_resources,
                                     VkExtent2D render_extent);
} // namespace Reaper
