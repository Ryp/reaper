////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/api/Vulkan.h"

namespace Reaper
{
struct FrameSyncResources
{
    VkFence drawFence;
};

struct ReaperRoot;
struct VulkanBackend;

FrameSyncResources create_frame_sync_resources(ReaperRoot& root, VulkanBackend& backend);
void               destroy_frame_sync_resources(VulkanBackend& backend, const FrameSyncResources& resources);
} // namespace Reaper
