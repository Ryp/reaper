////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Swapchain.h"

#include "common/Log.h"
#include "api/Vulkan.h"

namespace
{
    VkSurfaceFormatKHR vulkan_swapchain_choose_surface_format(std::vector<VkSurfaceFormatKHR> &surface_formats, VkSurfaceFormatKHR preferredFormat)
    {
        // If the list contains only one entry with undefined format
        // it means that there are no preferred surface formats and any can be chosen
        if ((surface_formats.size() == 1) && (surface_formats[0].format == VK_FORMAT_UNDEFINED))
            return preferredFormat;

        for (const VkSurfaceFormatKHR &surface_format : surface_formats)
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

    VkExtent2D vulkan_swapchain_choose_extent(VkSurfaceCapabilitiesKHR& surface_capabilities, VkExtent2D preferredExtent)
    {
        // Special value of surface extent is width == height == -1
        // If this is so we define the size by ourselves but it must fit within defined confines
        if (surface_capabilities.currentExtent.width == uint32_t(-1))
            return clamp(preferredExtent, surface_capabilities.minImageExtent, surface_capabilities.maxImageExtent);

        // Most of the cases we define size of the swap_chain images equal to current window's size
        return surface_capabilities.currentExtent;
    }

    VkSurfaceTransformFlagBitsKHR vulkan_swapchain_choose_tranform(VkSurfaceCapabilitiesKHR& surface_capabilities)
    {
        // Sometimes images must be transformed before they are presented (i.e. due to device's orienation
        // being other than default orientation)
        // If the specified transform is other than current transform, presentation engine will transform image
        // during presentation operation; this operation may hit performance on some platforms
        // Here we don't want any transformations to occur so if the identity transform is supported use it
        // otherwise just use the same transform as current transform
        if( surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ) {
            return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        } else {
            return surface_capabilities.currentTransform;
        }
    }

    VkPresentModeKHR vulkan_swapchain_choose_present_mode(std::vector<VkPresentModeKHR>& present_modes)
    {
        // Prefer MAILBOX over FIFO
        for (VkPresentModeKHR &present_mode : present_modes)
        {
            if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return present_mode;
        }

        for (VkPresentModeKHR &present_mode : present_modes)
        {
            if (present_mode == VK_PRESENT_MODE_FIFO_KHR)
                return present_mode;
        }

        Assert(false, "FIFO present mode is not supported by the swap chain!"); // Normally enforced by the API
        return VK_PRESENT_MODE_FIFO_KHR;
    }
}

using namespace vk;

void create_vulkan_swapchain(ReaperRoot& root, const VulkanBackend& backend, const SwapchainDescriptor& swapchainDesc, PresentationInfo& presentInfo)
{
    log_debug(root, "vulkan: creating swapchain");

    uint32_t formats_count;
    Assert(vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, presentInfo.surface, &formats_count, nullptr) == VK_SUCCESS);
    Assert(formats_count > 0);

    std::vector<VkSurfaceFormatKHR> surface_formats(formats_count);
    Assert(vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, presentInfo.surface, &formats_count, &surface_formats[0]) == VK_SUCCESS);

    log_debug(root, "vulkan: swapchain supports {} formats", formats_count);
    for (auto& format : surface_formats)
        log_debug(root, "- pixel format = {}, colorspace = {}", format.format, format.colorSpace); // TODO to_string() function

    VkSurfaceFormatKHR surfaceFormat = vulkan_swapchain_choose_surface_format(surface_formats, swapchainDesc.preferredFormat);

    if (surfaceFormat.format != swapchainDesc.preferredFormat.format || surfaceFormat.colorSpace != swapchainDesc.preferredFormat.colorSpace)
    {
        // TODO format to_string() function
        log_warning(root, "vulkan: incompatible swapchain format: pixel format = {}, colorspace = {}", swapchainDesc.preferredFormat.format, swapchainDesc.preferredFormat.colorSpace);
        log_warning(root, "- falling back to: pixel format = {}, colorspace = {}", surfaceFormat.format, surfaceFormat.colorSpace);
    }

    // Image count
    VkSurfaceCapabilitiesKHR surfaceCaps;
    Assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(backend.physicalDevice, presentInfo.surface, &surfaceCaps) == VK_SUCCESS);

    log_debug(root, "vulkan: swapchain image count support: min = {}, max = {}", surfaceCaps.minImageCount, surfaceCaps.maxImageCount);

    uint32_t imageCount = swapchainDesc.preferredImageCount;
    if (imageCount < surfaceCaps.minImageCount)
        imageCount = surfaceCaps.minImageCount;
    else if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
        imageCount = surfaceCaps.maxImageCount;

    if (imageCount != swapchainDesc.preferredImageCount)
        log_warning(root, "vulkan: swapchain image count {} is unsupported. falling back to {}", swapchainDesc.preferredImageCount, imageCount);

    Assert(imageCount > 0);
    Assert(imageCount < 4); // Seems like a reasonable limit

    // Present mode
    uint32_t presentModeCount;
    Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physicalDevice, presentInfo.surface, &presentModeCount, nullptr) == VK_SUCCESS);
    Assert(presentModeCount > 0);

    std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
    Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physicalDevice, presentInfo.surface, &presentModeCount, &availablePresentModes[0]) == VK_SUCCESS);

    log_debug(root, "vulkan: swapchain supports {} present modes", presentModeCount);
    for (auto& mode : availablePresentModes)
        log_debug(root, "- {}", mode); // TODO to_string() function

    VkPresentModeKHR present_mode = vulkan_swapchain_choose_present_mode(availablePresentModes);

    // Extent
    VkExtent2D extent = vulkan_swapchain_choose_extent(surfaceCaps, swapchainDesc.preferredExtent);

    if (extent.width != swapchainDesc.preferredExtent.width || extent.height != swapchainDesc.preferredExtent.height)
    {
        log_warning(root, "vulkan: swapchain extent {}x{} is not supported", swapchainDesc.preferredExtent.width, swapchainDesc.preferredExtent.height);
        log_warning(root, "- falling back to {}x{}", extent.width, extent.height);
    }

    // Usage flags
    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    Assert(surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "Vulkan API error");

    // Transform
    VkSurfaceTransformFlagBitsKHR transform = vulkan_swapchain_choose_tranform(surfaceCaps);

    VkSwapchainCreateInfoKHR swap_chain_create_info = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,// VkStructureType                sType
        nullptr,                                    // const void                    *pNext
        0,                                          // VkSwapchainCreateFlagsKHR      flags
        presentInfo.surface,                        // VkSurfaceKHR                   surface
        imageCount,                                 // uint32_t                       minImageCount
        surfaceFormat.format,                       // VkFormat                       imageFormat
        surfaceFormat.colorSpace,                   // VkColorSpaceKHR                imageColorSpace
        extent,                                     // VkExtent2D                     imageExtent
        1,                                          // uint32_t                       imageArrayLayers
        usageFlags,                                 // VkImageUsageFlags              imageUsage
        VK_SHARING_MODE_EXCLUSIVE,                  // VkSharingMode                  imageSharingMode
        0,                                          // uint32_t                       queueFamilyIndexCount
        nullptr,                                    // const uint32_t                *pQueueFamilyIndices
        transform,                                  // VkSurfaceTransformFlagBitsKHR  preTransform
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,          // VkCompositeAlphaFlagBitsKHR    compositeAlpha
        present_mode,                               // VkPresentModeKHR               presentMode
        VK_TRUE,                                    // VkBool32                       clipped
        VK_NULL_HANDLE                              // VkSwapchainKHR                 oldSwapchain
    };

    Assert(vkCreateSwapchainKHR(backend.device, &swap_chain_create_info, nullptr, &presentInfo.swapchain) == VK_SUCCESS);

    Assert(vkGetSwapchainImagesKHR(backend.device, presentInfo.swapchain, &presentInfo.imageCount, nullptr) == VK_SUCCESS);
    Assert(presentInfo.imageCount > 0);

    presentInfo.surfaceCaps = surfaceCaps;
    presentInfo.surfaceFormat = surfaceFormat;

    presentInfo.images.resize(presentInfo.imageCount);
    Assert(vkGetSwapchainImagesKHR(backend.device, presentInfo.swapchain, &presentInfo.imageCount, &presentInfo.images[0]) == VK_SUCCESS);

    // TODO Framebuffer and imageView creation
}

void destroy_vulkan_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    log_debug(root, "vulkan: destroying swapchain");

    //for (size_t i = 0; i < presentInfo.framebuffers.size(); ++i)
    //{
    //    vkDestroyImageView(backend.device, presentInfo.imageViews[i], nullptr);
    //    vkDestroyFramebuffer(backend.device, presentInfo.framebuffers[i], nullptr);
    //}
    presentInfo.imageViews.clear();
    presentInfo.framebuffers.clear();

    vkDestroySwapchainKHR(backend.device, presentInfo.swapchain, nullptr);
    presentInfo.swapchain = VK_NULL_HANDLE;
}

