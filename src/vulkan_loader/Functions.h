////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Vulkan.h"
#include "VulkanLoaderExport.h"

#include <core/DynamicLibrary.h>

namespace vk
{
#define REAPER_VK_EXPORTED_FUNCTION(func) REAPER_VULKAN_LOADER_API extern PFN_##func func;
#define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func) REAPER_VULKAN_LOADER_API extern PFN_##func func;
#define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func) REAPER_VULKAN_LOADER_API extern PFN_##func func;
#define REAPER_VK_DEVICE_LEVEL_FUNCTION(func) REAPER_VULKAN_LOADER_API extern PFN_##func func;
#include "SymbolHelper.inl"

REAPER_VULKAN_LOADER_API void vulkan_load_exported_functions(LibHandle vulkanLib);
REAPER_VULKAN_LOADER_API void vulkan_load_global_level_functions();
REAPER_VULKAN_LOADER_API void vulkan_load_instance_level_functions(VkInstance instance);
REAPER_VULKAN_LOADER_API void vulkan_load_device_level_functions(VkDevice device);
} // namespace vk
