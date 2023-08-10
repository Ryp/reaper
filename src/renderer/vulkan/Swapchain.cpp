////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Swapchain.h"

#include "Backend.h"

#include "api/VulkanStringConversion.h"
#include <vulkan_loader/Vulkan.h>

#include "common/Log.h"

#include "core/Assert.h"
#include "profiling/Scope.h"

namespace
{
VkSurfaceFormatKHR vulkan_swapchain_choose_surface_format(std::vector<VkSurfaceFormatKHR>& surface_formats,
                                                          VkSurfaceFormatKHR               preferredFormat)
{
    // If the list contains only one entry with undefined format
    // it means that there are no preferred surface formats and any can be chosen
    if ((surface_formats.size() == 1) && (surface_formats[0].format == VK_FORMAT_UNDEFINED))
        return preferredFormat;

    for (const VkSurfaceFormatKHR& surface_format : surface_formats)
    {
        if (surface_format.format == preferredFormat.format && surface_format.colorSpace == preferredFormat.colorSpace)
            return surface_format;
    }

    return surface_formats[0]; // Return first available format
}

uint32_t clamp(uint32_t v, uint32_t min, uint32_t max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    else
        return v;
}

VkExtent2D clamp(VkExtent2D value, VkExtent2D min, VkExtent2D max)
{
    return {
        clamp(value.width, min.width, max.width),
        clamp(value.height, min.height, max.height),
    };
}

VkExtent2D vulkan_swapchain_choose_extent(const VkSurfaceCapabilitiesKHR& surface_capabilities,
                                          VkExtent2D                      preferredExtent)
{
    // Special value of surface extent is width == height == -1
    // If this is so we define the size by ourselves but it must fit within defined confines
    if (surface_capabilities.currentExtent.width == uint32_t(-1))
        return clamp(preferredExtent, surface_capabilities.minImageExtent, surface_capabilities.maxImageExtent);

    // Most of the cases we define size of the swap_chain images equal to current window's size
    return surface_capabilities.currentExtent;
}

VkSurfaceTransformFlagBitsKHR vulkan_swapchain_choose_transform(const VkSurfaceCapabilitiesKHR& surface_capabilities)
{
    // Sometimes images must be transformed before they are presented (i.e. due to device's orienation
    // being other than default orientation)
    // If the specified transform is other than current transform, presentation engine will transform image
    // during presentation operation; this operation may hit performance on some platforms
    // Here we don't want any transformations to occur so if the identity transform is supported use it
    // otherwise just use the same transform as current transform
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        return surface_capabilities.currentTransform;
    }
}

VkPresentModeKHR vulkan_swapchain_choose_present_mode(std::vector<VkPresentModeKHR>& present_modes)
{
    // Prefer MAILBOX over FIFO
    for (VkPresentModeKHR& present_mode : present_modes)
    {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

// Overriding the view format lets us get the sRGB eotf for free in some cases.
// This needs VK_KHR_swapchain_mutable_format to work
VkFormat vulkan_swapchain_view_format_override(VkSurfaceFormatKHR surface_format)
{
    if (surface_format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
    {
        switch (surface_format.format)
        {
        case VK_FORMAT_B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
        default:
            break;
        }
    }

    return surface_format.format;
}
} // namespace

#if 0
#    include "renderer/window/Win32Window.h"
#endif

namespace Reaper
{
void configure_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend,
                                   const SwapchainDescriptor& swapchainDesc, PresentationInfo& presentInfo)
{
    log_debug(root, "vulkan: configuring wm swapchain");

    Assert(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(backend.physicalDevice, presentInfo.surface, &presentInfo.surfaceCaps)
        == VK_SUCCESS);

    VkSurfaceCapabilitiesKHR& surfaceCaps = presentInfo.surfaceCaps;

#if 0
    {
        Win32Window* win32Window = dynamic_cast<Win32Window*>(root.renderer->window);
        HMONITOR     monitor = MonitorFromWindow(win32Window->m_handle, MONITOR_DEFAULTTOPRIMARY);
        VkSurfaceFullScreenExclusiveWin32InfoEXT fullscreen_exclusive_info = {
            VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT,
            nullptr,
            monitor,
        };
        VkPhysicalDeviceSurfaceInfo2KHR             hey = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
                                               &fullscreen_exclusive_info, presentInfo.surface};
        VkSurfaceCapabilitiesFullScreenExclusiveEXT fullscreen_caps = {
            VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT, nullptr, {}};
        VkSurfaceCapabilities2KHR surfaceCaps2 = {VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, &fullscreen_caps, {}};

        Assert(vkGetPhysicalDeviceSurfaceCapabilities2KHR(backend.physicalDevice, &hey, &surfaceCaps2) == VK_SUCCESS);
        Assert(fullscreen_caps.fullScreenExclusiveSupported == VK_TRUE);
    }
#endif

    // Choose surface format
    VkSurfaceFormatKHR& surfaceFormat = presentInfo.surfaceFormat;
    {
        uint32_t formats_count;
        Assert(
            vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, presentInfo.surface, &formats_count, nullptr)
            == VK_SUCCESS);
        Assert(formats_count > 0);

        std::vector<VkSurfaceFormatKHR> surface_formats(formats_count);
        Assert(vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, presentInfo.surface, &formats_count,
                                                    &surface_formats[0])
               == VK_SUCCESS);

        log_debug(root, "vulkan: swapchain supports {} formats", formats_count);
        for (auto& format : surface_formats)
            log_debug(root, "- format = {}, colorspace = {}", GetFormatToString(format.format),
                      GetColorSpaceKHRToString(format.colorSpace));

        surfaceFormat = vulkan_swapchain_choose_surface_format(surface_formats, swapchainDesc.preferredFormat);

        if (surfaceFormat.format != swapchainDesc.preferredFormat.format
            || surfaceFormat.colorSpace != swapchainDesc.preferredFormat.colorSpace)
        {
            log_warning(root, "vulkan: incompatible swapchain format: format = {}, colorspace = {}",
                        GetFormatToString(swapchainDesc.preferredFormat.format),
                        GetColorSpaceKHRToString(swapchainDesc.preferredFormat.colorSpace));
            log_warning(root, "- falling back to: format = {}, colorspace = {}",
                        GetFormatToString(surfaceFormat.format), GetColorSpaceKHRToString(surfaceFormat.colorSpace));
        }

        presentInfo.view_format = vulkan_swapchain_view_format_override(surfaceFormat);

        log_debug(root, "vulkan: selecting swapchain format = {}, colorspace = {}",
                  GetFormatToString(surfaceFormat.format), GetColorSpaceKHRToString(surfaceFormat.colorSpace));
        log_debug(root, "vulkan: selecting swapchain view format = {}", GetFormatToString(presentInfo.view_format));
    }

    // Image count
    uint32_t imageCount = swapchainDesc.preferredImageCount;
    {
        log_debug(root, "vulkan: swapchain image count support: min = {}, max = {}", surfaceCaps.minImageCount,
                  surfaceCaps.maxImageCount);

        if (imageCount < surfaceCaps.minImageCount)
            imageCount = surfaceCaps.minImageCount;

        // Max count might be zero when uncapped
        if (imageCount > surfaceCaps.maxImageCount && surfaceCaps.maxImageCount > 0)
            imageCount = surfaceCaps.maxImageCount;

        if (imageCount != swapchainDesc.preferredImageCount)
        {
            log_warning(root, "vulkan: swapchain image count {} is unsupported. falling back to {}",
                        swapchainDesc.preferredImageCount, imageCount);
        }

        Assert(imageCount >= 2, "Swapchain should support at least double-buffering");
        Assert(imageCount <= 5, "Swapchain image count too large");

        presentInfo.imageCount = imageCount;
    }

    {
        uint32_t presentModeCount;
        Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physicalDevice, presentInfo.surface, &presentModeCount,
                                                         nullptr)
               == VK_SUCCESS);
        Assert(presentModeCount > 0);

        std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
        Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physicalDevice, presentInfo.surface, &presentModeCount,
                                                         availablePresentModes.data())
               == VK_SUCCESS);

        log_debug(root, "vulkan: swapchain supports {} present modes", presentModeCount);
        for (auto& mode : availablePresentModes)
            log_debug(root, "- {}", GetPresentModeKHRToString(mode));

        presentInfo.presentMode = vulkan_swapchain_choose_present_mode(availablePresentModes);
    }

    {
        VkExtent2D extent = vulkan_swapchain_choose_extent(surfaceCaps, swapchainDesc.preferredExtent);

        if (extent.width != swapchainDesc.preferredExtent.width
            || extent.height != swapchainDesc.preferredExtent.height)
        {
            log_warning(root, "vulkan: swapchain extent {}x{} is not supported", swapchainDesc.preferredExtent.width,
                        swapchainDesc.preferredExtent.height);
            log_warning(root, "- falling back to {}x{}", extent.width, extent.height);
        }
        presentInfo.surfaceExtent = extent;
    }

    // Usage flags
    // NOTE: Some flags might be added at runtime by external forces (driver, layers?)
    // and might trigger a validation warning down the line.
    // I suspect this is primarily done for frame capture purposes, like adding TRANSFER_SRC to allow copies.
    // It should be safe to ignore in this case.
    presentInfo.swapchainUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    Assert((surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0, "Vulkan API error");

    // Transform
    presentInfo.transform = vulkan_swapchain_choose_transform(surfaceCaps);
}

void create_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    REAPER_PROFILE_SCOPE_FUNC();
    log_debug(root, "vulkan: creating wm swapchain");

#if 0
    VkSurfaceFullScreenExclusiveInfoEXT fullscreen_info = {
        VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT,
        nullptr,
        VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT,
    };
#endif

    const std::vector<VkFormat> view_formats = {
        presentInfo.surfaceFormat.format, // Usual format if the swapchain was immutable
        presentInfo.view_format,          // Format to use the mutable feature with
    };

    const VkImageFormatListCreateInfo format_list = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
#if 0
        .pNext = &fullscreen_info,
#else
        .pNext = nullptr,
#endif
        .viewFormatCount = static_cast<u32>(view_formats.size()),
        .pViewFormats = view_formats.data(),
    };

    VkSwapchainCreateInfoKHR swap_chain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = &format_list,
        .flags = VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR,
        .surface = presentInfo.surface,
        .minImageCount = presentInfo.imageCount,
        .imageFormat = presentInfo.surfaceFormat.format,
        .imageColorSpace = presentInfo.surfaceFormat.colorSpace,
        .imageExtent = presentInfo.surfaceExtent,
        .imageArrayLayers = 1,
        .imageUsage = presentInfo.swapchainUsageFlags,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = presentInfo.transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentInfo.presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = presentInfo.swapchain,
    };

    Assert(vkCreateSwapchainKHR(backend.device, &swap_chain_create_info, nullptr, &presentInfo.swapchain)
           == VK_SUCCESS);

    u32 actualImageCount = 0;
    Assert(vkGetSwapchainImagesKHR(backend.device, presentInfo.swapchain, &actualImageCount, nullptr) == VK_SUCCESS);

    Assert(actualImageCount > 0);
    Assert(actualImageCount >= presentInfo.imageCount, "Invalid swapchain image count returned");

    presentInfo.images.resize(actualImageCount);
    Assert(vkGetSwapchainImagesKHR(backend.device, presentInfo.swapchain, &actualImageCount, &presentInfo.images[0])
           == VK_SUCCESS);

    if (actualImageCount != presentInfo.imageCount)
    {
        log_warning(root, "vulkan: {} swapchain images were asked but we got {}", presentInfo.imageCount,
                    actualImageCount);
    }

    // Since we don't control which swapchain image index we'll get when acquiring one,
    // we have to use the full amount of swapchain we get back.
    presentInfo.imageCount = actualImageCount;

    VkSemaphoreCreateInfo semaphore_create_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, // VkStructureType          sType
        nullptr,                                 // const void*              pNext
        0                                        // VkSemaphoreCreateFlags   flags
    };

    create_swapchain_views(backend, presentInfo);

    Assert(vkCreateSemaphore(backend.device, &semaphore_create_info, nullptr, &presentInfo.imageAvailableSemaphore)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created semaphore with handle: {}",
              static_cast<void*>(presentInfo.imageAvailableSemaphore));

    Assert(vkCreateSemaphore(backend.device, &semaphore_create_info, nullptr, &presentInfo.renderingFinishedSemaphore)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created semaphore with handle: {}",
              static_cast<void*>(presentInfo.renderingFinishedSemaphore));

    presentInfo.queue_swapchain_transition = true;
}

void destroy_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    REAPER_PROFILE_SCOPE_FUNC();
    log_debug(root, "vulkan: destroying wm swapchain");

    vkDestroySemaphore(backend.device, presentInfo.imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(backend.device, presentInfo.renderingFinishedSemaphore, nullptr);

    destroy_swapchain_views(backend, presentInfo);

    vkDestroySwapchainKHR(backend.device, presentInfo.swapchain, nullptr);
    presentInfo.swapchain = VK_NULL_HANDLE;
}

void resize_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo,
                                VkExtent2D extent)
{
    REAPER_PROFILE_SCOPE_FUNC();
    log_debug(root, "vulkan: resizing wm swapchain");

    // Destroy what needs to be
    Assert(vkDeviceWaitIdle(backend.device) == VK_SUCCESS);

    vkDestroySemaphore(backend.device, presentInfo.imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(backend.device, presentInfo.renderingFinishedSemaphore, nullptr);

    destroy_swapchain_views(backend, presentInfo);

    // Reconfigure even if we know most of what we expect/need
    SwapchainDescriptor swapchainDesc;
    swapchainDesc.preferredImageCount = presentInfo.imageCount;
    swapchainDesc.preferredFormat = presentInfo.surfaceFormat;
    swapchainDesc.preferredExtent = {extent.width, extent.height}; // New extent

    configure_vulkan_wm_swapchain(root, backend, swapchainDesc, presentInfo);

    // Do not destroy the current swapchain, we're recycling it
    Assert(presentInfo.swapchain != VK_NULL_HANDLE);

    VkSwapchainKHR old_swapchain = presentInfo.swapchain;

    create_vulkan_wm_swapchain(root, backend, presentInfo);

    Assert(presentInfo.swapchain != old_swapchain);

    vkDestroySwapchainKHR(backend.device, old_swapchain, nullptr);
}

void create_swapchain_views(const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    const size_t imgCount = presentInfo.imageCount;

    Assert(imgCount > 0);

    presentInfo.imageViews.resize(imgCount);

    for (size_t i = 0; i < imgCount; ++i)
    {
        VkImageViewCreateInfo image_view_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType                sType
            nullptr,                                  // const void                    *pNext
            0,                                        // VkImageViewCreateFlags         flags
            presentInfo.images[i],                    // VkImage                        image
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType                viewType
            presentInfo.view_format,                  // VkFormat                       format
            {
                // VkComponentMapping             components
                VK_COMPONENT_SWIZZLE_IDENTITY, // VkComponentSwizzle             r
                VK_COMPONENT_SWIZZLE_IDENTITY, // VkComponentSwizzle             g
                VK_COMPONENT_SWIZZLE_IDENTITY, // VkComponentSwizzle             b
                VK_COMPONENT_SWIZZLE_IDENTITY  // VkComponentSwizzle             a
            },
            {
                // VkImageSubresourceRange        subresourceRange
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags             aspectMask
                0,                         // uint32_t                       baseMipLevel
                1,                         // uint32_t                       levelCount
                0,                         // uint32_t                       baseArrayLayer
                1                          // uint32_t                       layerCount
            }};

        Assert(vkCreateImageView(backend.device, &image_view_create_info, nullptr, &presentInfo.imageViews[i])
               == VK_SUCCESS);
    }
}

void destroy_swapchain_views(const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    for (size_t i = 0; i < presentInfo.imageViews.size(); i++)
        vkDestroyImageView(backend.device, presentInfo.imageViews[i], nullptr);

    presentInfo.imageViews.clear();
}
} // namespace Reaper
