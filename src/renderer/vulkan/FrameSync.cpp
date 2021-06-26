////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameSync.h"

#include "SwapchainRendererBase.h"

#include "common/Log.h"

namespace Reaper
{
FrameSyncResources create_frame_sync_resources(ReaperRoot& root, VulkanBackend& backend)
{
    // Create fence
    VkFenceCreateInfo fenceInfo = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
        0 // Not signaled by default
    };

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
