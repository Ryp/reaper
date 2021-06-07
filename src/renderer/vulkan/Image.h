////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/AMDVulkanMemoryAllocator.h"

#include "api/Vulkan.h"

#include "renderer/format/PixelFormat.h"
#include "renderer/texture/GPUTextureProperties.h"

namespace Reaper
{
struct ReaperRoot;

struct ImageInfo
{
    VkImage              handle;
    VmaAllocation        allocation;
    GPUTextureProperties properties;
};

PixelFormat VulkanToPixelFormat(VkFormat format);

VkFormat              PixelFormatToVulkan(PixelFormat format);
VkSampleCountFlagBits SampleCountToVulkan(u32 sampleCount);
VkImageCreateFlags    GetVulkanCreateFlags(const GPUTextureProperties& properties);
VkImageUsageFlags     GetVulkanUsageFlags(u32 usageFlags);
u32                   GetUsageFlags(VkImageUsageFlags usageFlags);

ImageInfo   create_image(ReaperRoot& root, VkDevice device, const char* debug_string,
                         const GPUTextureProperties& properties, VmaAllocator& allocator);
VkImageView create_default_image_view(ReaperRoot& root, VkDevice device, const ImageInfo& image);
VkImageView create_depth_image_view(ReaperRoot& root, VkDevice device, const ImageInfo& image);

VkWriteDescriptorSet create_image_descriptor_write(VkDescriptorSet descriptorSet, u32 binding,
                                                   VkDescriptorType             descriptorType,
                                                   const VkDescriptorImageInfo* imageInfo);
} // namespace Reaper
