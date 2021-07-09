////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "api/Vulkan.h"

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
struct PresentationInfo;

struct SwapchainDescriptor
{
    uint32_t           preferredImageCount; // Can fallback on different modes
    VkSurfaceFormatKHR preferredFormat;     // Can fallback on supported format
    VkExtent2D         preferredExtent;     // Set this to the window's size if not used
};

// Window Manager API
void configure_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend,
                                   const SwapchainDescriptor& swapchainDesc, PresentationInfo& presentInfo);

void create_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo);
void destroy_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo);

void resize_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo,
                                VkExtent2D extent);

void create_swapchain_views(const VulkanBackend& backend, PresentationInfo& presentInfo);
void destroy_swapchain_views(const VulkanBackend& backend, PresentationInfo& presentInfo);
} // namespace Reaper
