////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameSync.h"

#include "Backend.h"
#include "Debug.h"
#include "api/AssertHelper.h"

namespace Reaper
{
FrameSyncResources create_frame_sync_resources(VulkanBackend& backend)
{
    const VkSemaphoreTypeCreateInfo semaphore_type_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
        .initialValue = 0,
    };

    const VkSemaphoreCreateInfo timeline_semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphore_type_create_info,
        .flags = VK_FLAGS_NONE,
    };

    VkSemaphore timeline_semaphore;
    AssertVk(vkCreateSemaphore(backend.device, &timeline_semaphore_create_info, nullptr, &timeline_semaphore));

    VulkanSetDebugName(backend.device, timeline_semaphore, "Main frame sync timeline semaphore");

    FrameSyncResources resources = {};

    resources.timeline_semaphore = timeline_semaphore;

    return resources;
}

void destroy_frame_sync_resources(VulkanBackend& backend, const FrameSyncResources& resources)
{
    vkDestroySemaphore(backend.device, resources.timeline_semaphore, nullptr);
}
} // namespace Reaper
