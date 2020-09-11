////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/api/Vulkan.h"

namespace Reaper
{
struct BlitPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

VkRenderPass create_main_raster_pass(ReaperRoot& /*root*/, VulkanBackend& backend,
                                     const GPUTextureProperties& depthProperties);

BlitPipelineInfo create_blit_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass);
} // namespace Reaper
