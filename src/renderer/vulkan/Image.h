////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/AMDVulkanMemoryAllocator.h"

#include "api/Vulkan.h"

#include "renderer/format/PixelFormat.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/allocator/GPUStackAllocator.h"

namespace Reaper
{
struct ReaperRoot;

struct ImageInfo
{
    VkImage              handle;
    VmaAllocation        allocation;
    GPUTextureProperties properties;
};

VkFormat              PixelFormatToVulkan(PixelFormat format);
VkSampleCountFlagBits SampleCountToVulkan(u32 sampleCount);
VkImageUsageFlags     GetVulkanUsageFlags(u32 usageFlags);
ImageInfo             create_image(ReaperRoot& root, VkDevice device, const char* debug_string,
                                   const GPUTextureProperties& properties, VmaAllocator& allocator);
VkImageView           create_default_image_view(VkDevice device, const ImageInfo& image);
VkImageView           create_depth_image_view(VkDevice device, const ImageInfo& image);
} // namespace Reaper
