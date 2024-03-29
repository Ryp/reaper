////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

// Let us declare API functions ourselves
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <core/Types.h>

// You can use this instead of 0 when passing empty flags to vulkan structs
constexpr u32 VK_FLAGS_NONE = 0;

#define REAPER_VK_USE_SWAPCHAIN_EXTENSIONS
#include "Functions.h"

// Global using for vulkan functions
using namespace vk;
