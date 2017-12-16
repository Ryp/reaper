////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Vulkan.h"

namespace vk
{
const char* GetPresentModeKHRToString(VkPresentModeKHR presentMode);
const char* GetMemoryPropertyFlagBitToString(VkMemoryPropertyFlags memoryFlag);
const char* GetColorSpaceKHRToString(VkColorSpaceKHR colorSpace);
const char* GetFormatToString(VkFormat format);
} // namespace vk
