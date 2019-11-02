////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2018 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "api/Vulkan.h"

#include "renderer/format/PixelFormat.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/allocator/GPUStackAllocator.h"

namespace Reaper
{
struct ImageInfo
{
    VkImage              handle;
    GPUAlloc             alloc;
    GPUTextureProperties properties;
};

VkFormat              PixelFormatToVulkan(PixelFormat format);
VkSampleCountFlagBits SampleCountToVulkan(u32 sampleCount);
VkImageUsageFlags     GetVulkanUsageFlags(u32 usageFlags);
ImageInfo   CreateVulkanImage(VkDevice device, const GPUTextureProperties& properties, GPUStackAllocator& allocator);
VkImageView create_default_image_view(VkDevice device, const ImageInfo& image);
} // namespace Reaper
