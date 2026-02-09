////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/ColorSpace.h"
#include "renderer/TransferFunction.h"

#include <vulkan_loader/Vulkan.h>

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

struct SwapchainFormat
{
    VkFormat        vk_format;
    VkColorSpaceKHR vk_color_space;
    VkFormat        vk_view_format;

    ColorSpace       color_space = ColorSpace::Unknown;
    TransferFunction transfer_function = TransferFunction::Unknown;
    bool             is_hdr = false;
};

SwapchainFormat create_surface_format(VkSurfaceFormatKHR surface_format);

// Window Manager API
void configure_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend,
                                   const SwapchainDescriptor& swapchainDesc, PresentationInfo& presentInfo);

void create_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo);
void destroy_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo);

void resize_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo,
                                VkExtent2D extent);
} // namespace Reaper
