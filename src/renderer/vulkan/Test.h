////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Buffer.h"
#include "Image.h"

#include "api/Vulkan.h"

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;

struct GlobalResources
{
    ImageInfo        image;
    VkImageView      imageView;
    VkDescriptorPool descriptorPool;
    VkCommandBuffer  gfxCmdBuffer;
};

REAPER_RENDERER_API
void vulkan_test(ReaperRoot& root, VulkanBackend& backend);
} // namespace Reaper
