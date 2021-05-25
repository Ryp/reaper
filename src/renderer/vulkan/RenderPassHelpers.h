////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "api/Vulkan.h"

#include <glm/glm.hpp>

namespace Reaper
{
inline VkViewport default_vk_viewport(VkRect2D output_rect)
{
    return VkViewport{static_cast<float>(output_rect.offset.x),
                      static_cast<float>(output_rect.offset.y),
                      static_cast<float>(output_rect.extent.width),
                      static_cast<float>(output_rect.extent.height),
                      0.0f,
                      1.0f};
}

inline VkRect2D default_vk_rect(VkExtent2D image_extent)
{
    return VkRect2D{{0, 0}, image_extent};
}

inline VkClearValue VkClearColor(const glm::vec4& color)
{
    VkClearValue clearValue;

    clearValue.color.float32[0] = color.x;
    clearValue.color.float32[1] = color.y;
    clearValue.color.float32[2] = color.z;
    clearValue.color.float32[3] = color.w;

    return clearValue;
}

inline VkClearValue VkClearDepthStencil(float depth, u32 stencil)
{
    VkClearValue clearValue;

    clearValue.depthStencil.depth = depth;
    clearValue.depthStencil.stencil = stencil;

    return clearValue;
}

struct GPUTextureProperties;

VkFramebufferAttachmentImageInfo get_attachment_info_from_image_properties(const GPUTextureProperties& properties,
                                                                           VkFormat* output_format_ptr);
} // namespace Reaper
