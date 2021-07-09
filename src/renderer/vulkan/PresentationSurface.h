////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "api/Vulkan.h"

namespace Reaper
{
class IWindow;

void vulkan_create_presentation_surface(VkInstance instance, VkSurfaceKHR& vkPresentationSurface, IWindow* window);

bool vulkan_queue_family_has_presentation_support(VkPhysicalDevice device, uint32_t queueFamilyIndex, IWindow* window);
} // namespace Reaper
