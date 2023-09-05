////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

namespace Reaper
{
struct SamplerResources
{
    VkSampler linear_clamp;
    VkSampler linear_black_border;
    VkSampler shadow_map_sampler;
    VkSampler diffuse_map_sampler;
};

struct VulkanBackend;

SamplerResources create_sampler_resources(VulkanBackend& backend);
void             destroy_sampler_resources(VulkanBackend& backend, SamplerResources& resources);
} // namespace Reaper
