////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "api/Vulkan.h"

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;

struct CullPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

CullPipelineInfo create_cull_pipeline(ReaperRoot& root, VulkanBackend& backend);
} // namespace Reaper
