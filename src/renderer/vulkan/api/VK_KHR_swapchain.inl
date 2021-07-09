////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkDestroySurfaceKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR)

#if defined(VK_USE_PLATFORM_WIN32_KHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceWin32PresentationSupportKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateWin32SurfaceKHR)
#elif defined(VK_USE_PLATFORM_XCB_KHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceXcbPresentationSupportKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateXcbSurfaceKHR)
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceXlibPresentationSupportKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateXlibSurfaceKHR)
#endif

REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateSwapchainKHR)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroySwapchainKHR)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetSwapchainImagesKHR)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkAcquireNextImageKHR)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkQueuePresentKHR)
