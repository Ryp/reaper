////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "common/ReaperRoot.h"

#include "SwapchainRendererBase.h"

namespace Reaper
{
struct SwapchainDescriptor
{
    uint32_t           preferredImageCount; // Can fallback on different modes
    VkSurfaceFormatKHR preferredFormat;     // Can fallback on supported format
    VkExtent2D         preferredExtent;     // Set this to the window's size if not used
};

void create_vulkan_swapchain(ReaperRoot& root, const VulkanBackend& backend, const SwapchainDescriptor& swapchainDesc,
                             PresentationInfo& presentInfo);
void destroy_vulkan_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo);

void create_swapchain_framebuffers(const VulkanBackend& backend, PresentationInfo& presentInfo);
void destroy_swapchain_framebuffers(const VulkanBackend& backend, PresentationInfo& presentInfo);

void create_swapchain_renderpass(const VulkanBackend& backend, PresentationInfo& presentInfo);
void destroy_swapchain_renderpass(const VulkanBackend& backend, PresentationInfo& presentInfo);
} // namespace Reaper
