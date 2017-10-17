////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Test.h"

#include "Memory.h"
#include "SwapchainRendererBase.h"
#include "api/Vulkan.h"
#include "api/VulkanStringConversion.h"

#include "renderer/texture/GPUTextureProperties.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/BitTricks.h"

using namespace vk;

namespace Reaper
{
namespace
{
    VkFormat PixelFormatToVulkan(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::R16G16B16A16_UNORM:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case PixelFormat::R16G16B16A16_SFLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case PixelFormat::R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::R8G8B8A8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case PixelFormat::B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::BC2_UNORM_BLOCK:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case PixelFormat::Unknown:
        default:
            AssertUnreachable();
        }
        return VK_FORMAT_UNDEFINED;
    }

    VkSampleCountFlagBits SampleCountToVulkan(u32 sampleCount)
    {
        Assert(sampleCount > 0);
        Assert(isPowerOfTwo(sampleCount));
        return static_cast<VkSampleCountFlagBits>(sampleCount);
    }
} // namespace

void vulkan_test(ReaperRoot& root, VulkanBackend& backend)
{
    log_info(root, "test ////////////////////////////////////////");

    GPUTextureProperties properties = DefaultGPUTextureProperties(800, 600, PixelFormat::R16G16B16A16_UNORM);

    VkExtent3D extent = {properties.width, properties.height, properties.depth};

    VkImageTiling     tilingMode = VK_IMAGE_TILING_OPTIMAL;
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                   nullptr,
                                   0,
                                   VK_IMAGE_TYPE_2D,
                                   PixelFormatToVulkan(properties.format),
                                   extent,
                                   properties.mipCount,
                                   properties.layerCount,
                                   SampleCountToVulkan(properties.sampleCount),
                                   tilingMode,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                   VK_SHARING_MODE_EXCLUSIVE,
                                   0,
                                   nullptr,
                                   VK_IMAGE_LAYOUT_UNDEFINED};

    log_debug(root, "vulkan: creating new image: extent = {}x{}x{}, format = {}", extent.width, extent.height,
              extent.depth, imageInfo.format);
    log_debug(root, "- mips = {}, layers = {}, samples = {}", imageInfo.mipLevels, imageInfo.arrayLayers,
              imageInfo.samples);

    VkImage image = VK_NULL_HANDLE;
    Assert(vkCreateImage(backend.device, &imageInfo, nullptr, &image) == VK_SUCCESS);

    log_debug(root, "vulkan: created image with handle: {}", static_cast<void*>(image));

    // check mem requirements
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(backend.device, image, &memoryRequirements);

    log_debug(root, "- image memory requirements: size = {}, alignment = {}, types = {:#b}", memoryRequirements.size,
              memoryRequirements.alignment, memoryRequirements.memoryTypeBits);

    VkMemoryPropertyFlags requiredMemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t memoryTypeIndex =
        getMemoryType(backend.physicalDevice, memoryRequirements.memoryTypeBits, requiredMemoryType);

    Assert(memoryTypeIndex < backend.physicalDeviceInfo.memory.memoryTypeCount, "invalid memory type index");
    {
        VkMemoryType& memoryType = backend.physicalDeviceInfo.memory.memoryTypes[memoryTypeIndex];

        log_debug(root, "vulkan: selecting memory heap {} with these properties:", memoryType.heapIndex);
        for (u32 i = 0; i < sizeof(VkMemoryPropertyFlags) * 8; i++)
        {
            VkMemoryPropertyFlags flag = 1 << i;

            if (memoryType.propertyFlags & flag)
                log_debug(root, "- {}", GetMemoryPropertyFlagBitToString(flag));
        }
    }

    // It's fine here but when allocating from big buffer we need to align correctly the offset.
    VkDeviceSize allocSize = memoryRequirements.size + memoryRequirements.alignment - 1; // ALIGN PROPERLY

    // back with da memory
    VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, allocSize, memoryTypeIndex};

    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    Assert(vkAllocateMemory(backend.device, &memAllocInfo, nullptr, &imageMemory) == VK_SUCCESS);

    log_debug(root, "vulkan: allocated memory with handle: {}", static_cast<void*>(imageMemory));

    // bind resource with memory
    // VkDeviceSize memAlignedOffset = alignOffset(0, memoryRequirements.alignment); // take memreqs into account
    // vkAllocateMemory gives one aligned offset by default
    VkDeviceSize memAlignedOffset = 0;
    Assert(vkBindImageMemory(backend.device, image, imageMemory, memAlignedOffset) == VK_SUCCESS);

    // image view
    VkImageSubresourceRange viewRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageViewCreateInfo imageViewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0, // reserved
        image,
        VK_IMAGE_VIEW_TYPE_2D,
        imageInfo.format, // reuse same format
        VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY},
        viewRange};

    VkImageView imageView = VK_NULL_HANDLE;
    Assert(vkCreateImageView(backend.device, &imageViewInfo, nullptr, &imageView) == VK_SUCCESS);

    log_debug(root, "vulkan: created image view with handle: {}", static_cast<void*>(imageView));

    // cleanup
    vkFreeMemory(backend.device, imageMemory, nullptr);
    vkDestroyImageView(backend.device, imageView, nullptr);
    vkDestroyImage(backend.device, image, nullptr);

    log_info(root, "test ////////////////////////////////////////");
}
} // namespace Reaper
