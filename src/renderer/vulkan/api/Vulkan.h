////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_VULKAN_INCLUDED
#define REAPER_VULKAN_INCLUDED

#if defined(REAPERGL_PLATFORM_LINUX) || defined(REAPERGL_PLATFORM_MACOSX)
    #define REAPER_VK_LIB_NAME "libvulkan.so"
#elif defined(REAPERGL_PLATFORM_WINDOWS)
    #define REAPER_VK_LIB_NAME "vulkan-1.dll"
#endif

#define VK_USE_PLATFORM_XCB_KHR

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "VulkanFunctions.h"

#endif // REAPER_VULKAN_INCLUDED
