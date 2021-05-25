////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "RenderPassHelpers.h"

#include "Image.h"

#include "renderer/texture/GPUTextureProperties.h"

namespace Reaper
{
VkFramebufferAttachmentImageInfo get_attachment_info_from_image_properties(const GPUTextureProperties& properties,
                                                                           VkFormat* output_format_ptr)
{
    *output_format_ptr = PixelFormatToVulkan(properties.format);

    return VkFramebufferAttachmentImageInfo{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
        nullptr,
        GetVulkanCreateFlags(properties),           // VkImageCreateFlags    flags;
        GetVulkanUsageFlags(properties.usageFlags), // VkImageUsageFlags     usage;
        properties.width,                           // uint32_t              width;
        properties.height,                          // uint32_t              height;
        1,                                          // uint32_t              layerCount;
        1,                                          // uint32_t              viewFormatCount;
        output_format_ptr                           // const VkFormat*       pViewFormats;
    };
}
} // namespace Reaper
