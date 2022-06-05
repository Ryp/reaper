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
const char* GetResultToString(VkResult result);
const char* GetPresentModeKHRToString(VkPresentModeKHR presentMode);
const char* GetMemoryPropertyFlagBitToString(VkMemoryPropertyFlags memoryFlag);
const char* GetColorSpaceKHRToString(VkColorSpaceKHR colorSpace);
const char* GetFormatToString(VkFormat format);
const char* GetImageLayoutToString(VkImageLayout layout);
} // namespace vk
