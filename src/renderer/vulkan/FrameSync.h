////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

namespace Reaper
{
struct FrameSyncResources
{
    VkSemaphore timeline_semaphore;
};

struct VulkanBackend;

FrameSyncResources create_frame_sync_resources(VulkanBackend& backend);
void               destroy_frame_sync_resources(VulkanBackend& backend, const FrameSyncResources& resources);
} // namespace Reaper
