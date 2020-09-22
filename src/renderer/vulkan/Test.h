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
    VkCommandBuffer gfxCmdBuffer;
};

void debug_memory_heap_properties(ReaperRoot& root, const VulkanBackend& backend, uint32_t memoryTypeIndex);

REAPER_RENDERER_API
void vulkan_test(ReaperRoot& root, VulkanBackend& backend);
} // namespace Reaper
