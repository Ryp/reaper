
////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2026 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Semaphore.h"

#include "Backend.h"
#include "Debug.h"
#include "api/AssertHelper.h"

#include <core/Assert.h>

namespace Reaper
{
VkSemaphore create_semaphore(const VulkanBackend& backend, const VkSemaphoreCreateInfo& create_info,
                             const char* debug_name)
{
    VkSemaphore semaphore;
    AssertVk(vkCreateSemaphore(backend.device, &create_info, nullptr, &semaphore));

    VulkanSetDebugName(backend.device, semaphore, debug_name);

    return semaphore;
}
} // namespace Reaper
