////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameSync.h"

#include "Backend.h"

#include "common/Log.h"

namespace Reaper
{
FrameSyncResources create_frame_sync_resources(ReaperRoot& root, VulkanBackend& backend)
{
    // Create fence signaled by default, so we don't have to make it a special case when waiting for the last frame at
    // the first frame
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};

    VkFence drawFence = VK_NULL_HANDLE;
    vkCreateFence(backend.device, &fenceInfo, nullptr, &drawFence);
    log_debug(root, "vulkan: created fence with handle: {}", static_cast<void*>(drawFence));

    FrameSyncResources resources = {};

    resources.drawFence = drawFence;

    return resources;
}

void destroy_frame_sync_resources(VulkanBackend& backend, const FrameSyncResources& resources)
{
    vkDestroyFence(backend.device, resources.drawFence, nullptr);
}
} // namespace Reaper
