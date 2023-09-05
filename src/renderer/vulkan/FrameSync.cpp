////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameSync.h"

#include "Backend.h"

namespace Reaper
{
FrameSyncResources create_frame_sync_resources(VulkanBackend& backend)
{
    // Create fence signaled by default, so we don't have to make it a special case when waiting for the last frame at
    // the first frame
    const VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkFence draw_fence = VK_NULL_HANDLE;
    vkCreateFence(backend.device, &fenceInfo, nullptr, &draw_fence);

    FrameSyncResources resources = {};

    resources.draw_fence = draw_fence;

    return resources;
}

void destroy_frame_sync_resources(VulkanBackend& backend, const FrameSyncResources& resources)
{
    vkDestroyFence(backend.device, resources.draw_fence, nullptr);
}
} // namespace Reaper
