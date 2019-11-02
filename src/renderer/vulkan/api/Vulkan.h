////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

// Let us declare API functions ourselves
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// You can use this instead of 0 when passing empty flags to vulkan structs
#define VK_FLAGS_NONE 0

// Name of the library to dynamically load to get vulkan symbols
#if defined(REAPER_PLATFORM_LINUX) || defined(REAPER_PLATFORM_MACOSX)
#    define REAPER_VK_LIB_NAME "libvulkan.so"
#elif defined(REAPER_PLATFORM_WINDOWS)
#    define REAPER_VK_LIB_NAME "vulkan-1.dll"
#endif

// Version of the API to query when loading vulkan symbols
#define REAPER_VK_API_VERSION VK_API_VERSION_1_0

// Decide which swapchain extension to use
#if defined(VK_USE_PLATFORM_WIN32_KHR)
#    define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#    define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#    define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif

#define REAPER_VK_USE_SWAPCHAIN_EXTENSIONS
#include "VulkanFunctions.h"

// Global using for vulkan functions
using namespace vk;
