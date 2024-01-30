////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

namespace vk
{
const char* vk_to_string(VkPhysicalDeviceType deviceType);
const char* vk_to_string(VkResult result);
const char* vk_to_string(VkPresentModeKHR presentMode);
const char* vk_to_string(VkColorSpaceKHR colorSpace);
const char* vk_to_string(VkFormat format);
const char* vk_to_string(VkSampleCountFlagBits sample_count);
const char* vk_to_string(VkImageLayout layout);
const char* vk_to_string(VkObjectType type);
const char* vk_to_string(VkMemoryPropertyFlags memory_property_flags);
} // namespace vk
