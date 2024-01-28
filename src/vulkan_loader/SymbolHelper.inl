////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// Exported functions
#ifndef REAPER_VK_EXPORTED_FUNCTION
#    define REAPER_VK_EXPORTED_FUNCTION(func)
#endif

REAPER_VK_EXPORTED_FUNCTION(vkGetInstanceProcAddr)

// Global level functions
#ifndef REAPER_VK_GLOBAL_LEVEL_FUNCTION
#    define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func)
#endif

REAPER_VK_GLOBAL_LEVEL_FUNCTION(vkCreateInstance)
REAPER_VK_GLOBAL_LEVEL_FUNCTION(vkEnumerateInstanceExtensionProperties)
REAPER_VK_GLOBAL_LEVEL_FUNCTION(vkEnumerateInstanceLayerProperties)

// Instance level functions
#ifndef REAPER_VK_INSTANCE_LEVEL_FUNCTION
#    define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func)
#endif

REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkDestroyInstance)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkEnumeratePhysicalDevices)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(
    vkGetPhysicalDeviceProperties) // NOTE: deprecated by vkGetPhysicalDeviceProperties2 but kept for Tracy
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceProperties2)
// REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceFeatures) NOTE: deprecated by vkGetPhysicalDeviceFeatures2
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceFeatures2)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkCreateDevice)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetDeviceProcAddr)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkEnumerateDeviceExtensionProperties)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceMemoryProperties2)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceFormatProperties)
REAPER_VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceImageFormatProperties2)

// Device level functions
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
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkFreeDescriptorSets)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkBeginCommandBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkResetCommandBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkResetCommandPool)
// REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdPipelineBarrier) NOTE: deprecated by vkCmdPipelineBarrier2
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdClearColorImage)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkEndCommandBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkUpdateDescriptorSets)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBindDescriptorSets)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateSemaphore)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroySemaphore)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkSignalSemaphore)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkWaitSemaphores)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetSemaphoreCounterValue)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateEvent)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyEvent)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkQueueSubmit) // NOTE: deprecated by vkQueueSubmit2
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkFreeCommandBuffers)
// REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateRenderPass) NOTE: deprecated by vkCreateRenderPass2
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateRenderPass2)
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
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateComputePipelines)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyPipeline)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkInvalidateMappedMemoryRanges)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBeginRenderPass)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdEndRenderPass)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBindPipeline)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetViewport)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetScissor)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDraw)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDrawIndexed)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDrawIndexedIndirect)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDrawIndexedIndirectCount)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDrawIndirectCount)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDispatch)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdDispatchIndirect)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateImage)
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
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdFillBuffer)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdPushConstants)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateFence)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyFence)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkResetFences)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetFenceStatus)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkWaitForFences)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateSampler)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroySampler)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkResetDescriptorPool)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBeginRendering)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdEndRendering)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetDeviceBufferMemoryRequirements)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetDeviceImageMemoryRequirements)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetDeviceImageSparseMemoryRequirements)
// REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBlitImage) NOTE: deprecated by vkCmdBlitImage2
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBlitImage2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyBuffer) // NOTE: deprecated by vkCmdCopyBuffer2
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyBuffer2)
// REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyBufferToImage) NOTE: deprecated by vkCmdCopyBufferToImage2
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyBufferToImage2)
// REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyImage) NOTE: deprecated by vkCmdCopyImage2
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyImage2)
// REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyImageToBuffer) NOTE: deprecated by vkCmdCopyImageToBuffer2
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyImageToBuffer2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdResolveImage2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdPipelineBarrier2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdResetEvent2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetEvent2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdWaitEvents2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdWriteTimestamp2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkQueueSubmit2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetEventStatus)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetQueryPoolResults)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateQueryPool)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkDestroyQueryPool)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdResetQueryPool)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdWriteTimestamp)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkFlushMappedMemoryRanges)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCreateRenderPass)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyBufferToImage)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdPipelineBarrier)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetBufferMemoryRequirements2)
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkGetImageMemoryRequirements2);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkBindBufferMemory2);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkBindImageMemory2);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdBindVertexBuffers2);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetCullMode);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetDepthBoundsTestEnable);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetDepthCompareOp);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetDepthTestEnable);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetDepthWriteEnable);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetFrontFace);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetPrimitiveTopology);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetScissorWithCount);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetStencilOp);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetStencilTestEnable);
REAPER_VK_DEVICE_LEVEL_FUNCTION(vkCmdSetViewportWithCount);

#include "extension/VK_EXT_calibrated_timestamps.inl"
#include "extension/VK_EXT_sample_locations.inl"
#include "extension/VK_KHR_get_surface_capabilities2.inl"

#if REAPER_DEBUG
#    include "extension/VK_EXT_debug_utils.inl"
#endif

#if defined(REAPER_VK_USE_SWAPCHAIN_EXTENSIONS)
#    include "extension/VK_KHR_swapchain.inl"
#endif

#if defined(REAPER_VK_USE_DISPLAY_EXTENSIONS)
#    include "extension/VK_KHR_display.inl"
#endif

#undef REAPER_VK_EXPORTED_FUNCTION
#undef REAPER_VK_GLOBAL_LEVEL_FUNCTION
#undef REAPER_VK_INSTANCE_LEVEL_FUNCTION
#undef REAPER_VK_DEVICE_LEVEL_FUNCTION
