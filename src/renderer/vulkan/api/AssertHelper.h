////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <fmt/format.h>

#include "VulkanStringConversion.h"
#include "core/Assert.h"

// NOTE: It's really important to evaluate 'vk_call' only once
#define AssertVk(vk_call)                                                                     \
    do                                                                                        \
    {                                                                                         \
        const VkResult result = vk_call;                                                      \
        AssertImpl(__FILE__, __FUNCTION__, __LINE__, result == VK_SUCCESS,                    \
                   fmt::format("vulkan call failed with result '{}'", vk_to_string(result))); \
    } while (false)
