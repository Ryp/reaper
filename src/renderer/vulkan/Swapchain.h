////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_SWAPCHAIN_INCLUDED
#define REAPER_SWAPCHAIN_INCLUDED

#include "common/ReaperRoot.h"

#include "SwapchainRendererBase.h"

struct SwapchainDescriptor
{
};

void create_vulkan_swapchain(ReaperRoot& root, const VulkanBackend& backend, const SwapchainDescriptor& swapchainDesc, PresentationInfo& presentInfo);
void destroy_vulkan_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo);

#endif // REAPER_SWAPCHAIN_INCLUDED

