////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_VULKAN_INCLUDED
#define REAPER_VULKAN_INCLUDED

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#if defined(REAPER_PLATFORM_LINUX) || defined(REAPER_PLATFORM_MACOSX)
    #define REAPER_VK_LIB_NAME "libvulkan.so"
#elif defined(REAPER_PLATFORM_WINDOWS)
    #define REAPER_VK_LIB_NAME "vulkan-1.dll"
#endif

#define REAPER_VK_API_VERSION VK_MAKE_VERSION(0, 0, 0)

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    #define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    #include <Windows.h>
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    #define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
    #include <xcb/xcb.h>
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    #define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#endif

#define REAPER_VK_USE_SWAPCHAIN_EXTENSIONS
#include "VulkanFunctions.h"

#endif // REAPER_VULKAN_INCLUDED
