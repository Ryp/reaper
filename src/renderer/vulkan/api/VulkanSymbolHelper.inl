////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// ************************************************************ //
// Exported functions                                           //
//                                                              //
// These functions are always exposed by vulkan libraries.      //
// ************************************************************ //

#ifndef REAPER_VK_EXPORTED_FUNCTION
#    define REAPER_VK_EXPORTED_FUNCTION(func)
#endif

REAPER_VK_EXPORTED_FUNCTION(vkGetInstanceProcAddr)

#undef REAPER_VK_EXPORTED_FUNCTION

// ************************************************************ //
// Global level functions                                       //
//                                                              //
// They allow checking what instance extensions are available   //
// and allow creation of a Vulkan Instance.                     //
// ************************************************************ //

#ifndef REAPER_VK_GLOBAL_LEVEL_FUNCTION
#    define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func)
#endif

REAPER_VK_GLOBAL_LEVEL_FUNCTION(vkCreateInstance)
REAPER_VK_GLOBAL_LEVEL_FUNCTION(vkEnumerateInstanceExtensionProperties)
REAPER_VK_GLOBAL_LEVEL_FUNCTION(vkEnumerateInstanceLayerProperties)

#undef REAPER_VK_GLOBAL_LEVEL_FUNCTION

// ************************************************************ //
// Instance level functions                                     //
//                                                              //
// These functions allow for device queries and creation.       //
// They help choose which device is well suited for our needs.  //
// ************************************************************ //

#ifndef REAPER_VK_INSTANCE_LEVEL_FUNCTION
#    define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func)
#endif

REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkDestroyInstance)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkEnumeratePhysicalDevices)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceProperties)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceFeatures)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateDevice)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetDeviceProcAddr)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkEnumerateDeviceExtensionProperties)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceFormatProperties)
// From extensions
#if defined(REAPER_VK_USE_SWAPCHAIN_EXTENSIONS)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkDestroySurfaceKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR)
#    if defined(VK_USE_PLATFORM_WIN32_KHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceWin32PresentationSupportKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateWin32SurfaceKHR)
#    elif defined(VK_USE_PLATFORM_XCB_KHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceXcbPresentationSupportKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateXcbSurfaceKHR)
#    elif defined(VK_USE_PLATFORM_XLIB_KHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceXlibPresentationSupportKHR)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateXlibSurfaceKHR)
#    endif
#endif

#if defined(REAPER_DEBUG)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateDebugReportCallbackEXT)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkDebugReportMessageEXT)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkDestroyDebugReportCallbackEXT)
#endif

#undef REAPER_VK_INSTANCE_LEVEL_FUNCTION

// ************************************************************ //
// Device level functions                                       //
//                                                              //
// These functions are used mainly for drawing                  //
// ************************************************************ //

#ifndef REAPER_VK_DEVICE_LEVEL_FUNCTION
#    define REAPER_VK_DEVICE_LEVEL_FUNCTION(func)
#endif

REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetDeviceQueue)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkQueueWaitIdle)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyDevice)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDeviceWaitIdle)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateDescriptorPool)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyDescriptorPool)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateDescriptorSetLayout)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyDescriptorSetLayout)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateCommandPool)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyCommandPool)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkAllocateCommandBuffers)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkAllocateDescriptorSets)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkBeginCommandBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdPipelineBarrier)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdClearColorImage)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkEndCommandBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkUpdateDescriptorSets)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBindDescriptorSets)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateSemaphore)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroySemaphore)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkQueueSubmit)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkFreeCommandBuffers)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateRenderPass)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyRenderPass)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateImageView)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyImageView)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateFramebuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyFramebuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateShaderModule)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyShaderModule)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreatePipelineLayout)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyPipelineLayout)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateGraphicsPipelines)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyPipeline)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBeginRenderPass)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdEndRenderPass)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBindPipeline)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDraw)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDrawIndexed)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateImage)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyImage)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyImage)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetImageMemoryRequirements)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkAllocateMemory)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkBindImageMemory)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkFreeMemory)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetBufferMemoryRequirements)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkBindBufferMemory)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkMapMemory)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkUnmapMemory)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBindVertexBuffers)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBindIndexBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdPushConstants)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyBufferToImage)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateFence)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyFence)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkResetFences)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkWaitForFences)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateSampler)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroySampler)
// From extensions
#if defined(REAPER_VK_USE_SWAPCHAIN_EXTENSIONS)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateSwapchainKHR)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroySwapchainKHR)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetSwapchainImagesKHR)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkAcquireNextImageKHR)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkQueuePresentKHR)
#endif

#undef REAPER_VK_DEVICE_LEVEL_FUNCTION
