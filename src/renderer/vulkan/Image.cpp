#include "Image.h"

#include "SwapchainRendererBase.h"

#include "renderer/texture/GPUTextureProperties.h"

namespace Reaper
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

VkImageUsageFlags GetVulkanUsageFlags(u32 usageFlags)
{
    VkImageUsageFlags flags = 0;

    flags |= (usageFlags & GPUTextureUsage::TransferSrc) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::TransferDst) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::Sampled) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::Storage) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::ColorAttachment) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::DepthStencilAttachment) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::TransientAttachment) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::InputAttachment) ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0;

    return flags;
}

ImageInfo CreateVulkanImage(VkDevice device, const GPUTextureProperties& properties, GPUStackAllocator& allocator)
{
    const VkExtent3D    extent = {properties.width, properties.height, properties.depth};
    const VkImageTiling tilingMode = VK_IMAGE_TILING_OPTIMAL;

    const VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                         nullptr,
                                         0,
                                         VK_IMAGE_TYPE_2D,
                                         PixelFormatToVulkan(properties.format),
                                         extent,
                                         properties.mipCount,
                                         properties.layerCount,
                                         SampleCountToVulkan(properties.sampleCount),
                                         tilingMode,
                                         GetVulkanUsageFlags(properties.usageFlags),
                                         VK_SHARING_MODE_EXCLUSIVE,
                                         0,
                                         nullptr,
                                         VK_IMAGE_LAYOUT_UNDEFINED};

    VkImage image = VK_NULL_HANDLE;
    Assert(vkCreateImage(device, &imageInfo, nullptr, &image) == VK_SUCCESS);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image, &memoryRequirements);

    const GPUAlloc alloc = allocator.alloc(memoryRequirements);

    Assert(vkBindImageMemory(device, image, alloc.memory, alloc.offset) == VK_SUCCESS);

    return {image, alloc, properties};
}

VkImageView create_default_image_view(VkDevice device, const ImageInfo& image)
{
    VkImageSubresourceRange viewRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, image.properties.mipCount, 0,
                                         image.properties.layerCount};

    VkImageViewCreateInfo imageViewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0, // reserved
        image.handle,
        VK_IMAGE_VIEW_TYPE_2D,
        PixelFormatToVulkan(image.properties.format),
        VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY},
        viewRange};

    VkImageView imageView = VK_NULL_HANDLE;
    Assert(vkCreateImageView(device, &imageViewInfo, nullptr, &imageView) == VK_SUCCESS);

    return imageView;
}
} // namespace Reaper
