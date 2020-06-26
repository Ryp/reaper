////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

// Let us declare API functions ourselves
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// You can use this instead of 0 when passing empty flags to vulkan structs
#define VK_FLAGS_NONE 0

#define REAPER_VK_USE_SWAPCHAIN_EXTENSIONS
#include "VulkanFunctions.h"

// Global using for vulkan functions
using namespace vk;
