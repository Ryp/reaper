////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VulkanStringConversion.h"

namespace vk
{
const char* GetPresentModeKHRToString(VkPresentModeKHR presentMode)
{
    switch (presentMode)
    {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "VK_PRESENT_MODE_IMMEDIATE_KHR";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "VK_PRESENT_MODE_MAILBOX_KHR";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "VK_PRESENT_MODE_FIFO_KHR";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
    default:
        AssertUnreachable();
    }
    return "Unknown";
}

const char* GetMemoryPropertyFlagBitToString(VkMemoryPropertyFlags memoryFlag)
{
    switch (memoryFlag)
    {
    case VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT:
        return "VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT";
    case VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT:
        return "VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT";
    case VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
        return "VK_MEMORY_PROPERTY_HOST_COHERENT_BIT";
    case VK_MEMORY_PROPERTY_HOST_CACHED_BIT:
        return "VK_MEMORY_PROPERTY_HOST_CACHED_BIT";
    case VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT:
        return "VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT";
    default:
        AssertUnreachable();
    }
    return "Unknown";
}
} // namespace vk
