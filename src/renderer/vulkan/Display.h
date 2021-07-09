////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;

// Still in experimental state.
// This is not supported by a lot of devices.
void create_vulkan_display_swapchain(ReaperRoot& root, const VulkanBackend& backend);
void destroy_vulkan_display_swapchain(ReaperRoot& root, const VulkanBackend& backend);
} // namespace Reaper
