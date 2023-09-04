////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/AMDVulkanMemoryAllocator.h"

#include <vulkan_loader/Vulkan.h>

#include "renderer/format/PixelFormat.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/texture/GPUTextureView.h"

namespace Reaper
{
struct ReaperRoot;

struct GPUTexture
{
    VkImage       handle;
    VmaAllocation allocation;
};

PixelFormat VulkanToPixelFormat(VkFormat format);

VkFormat                PixelFormatToVulkan(PixelFormat format);
VkSampleCountFlagBits   SampleCountToVulkan(u32 sampleCount);
VkImageCreateFlags      GetVulkanCreateFlags(const GPUTextureProperties& properties);
VkImageUsageFlags       GetVulkanUsageFlags(u32 usageFlags);
u32                     GetUsageFlags(VkImageUsageFlags usageFlags);
VkImageAspectFlags      GetVulkanImageAspectFlags(u32 aspect);
VkImageSubresourceRange GetVulkanImageSubresourceRange(const GPUTextureSubresource& subresource);

GPUTexture  create_image(ReaperRoot& root, VkDevice device, const char* debug_string,
                         const GPUTextureProperties& properties, VmaAllocator& allocator);
VkImageView create_image_view(VkDevice device, VkImage image, const GPUTextureView& view);

} // namespace Reaper
