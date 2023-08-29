////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "vulkan_loader/Vulkan.h"

namespace Reaper
{
struct ReaperRoot;

VkDebugUtilsMessengerEXT vulkan_create_debug_callback(VkInstance instance, ReaperRoot& root);
void vulkan_destroy_debug_callback(VkInstance instance, VkDebugUtilsMessengerEXT debug_utils_messenger);
} // namespace Reaper
