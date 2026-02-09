////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2026 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

namespace Reaper
{
struct VulkanBackend;

VkSemaphore create_semaphore(const VulkanBackend& backend, const VkSemaphoreCreateInfo& create_info,
                             const char* debug_name);
} // namespace Reaper
