////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Swapchain.h"

#include "api/Vulkan.h"

namespace
{
    VkSurfaceFormatKHR GetSwapChainFormat(std::vector<VkSurfaceFormatKHR> &surface_formats)
    {
        // If the list contains only one entry with undefined format
        // it means that there are no preferred surface formats and any can be chosen
        if( (surface_formats.size() == 1) && (surface_formats[0].format == VK_FORMAT_UNDEFINED) ) {
            return{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        }

        // Check if list contains most widely used R8 G8 B8 A8 format
        // with nonlinear color space
        for( VkSurfaceFormatKHR &surface_format : surface_formats ) {
            if( surface_format.format == VK_FORMAT_R8G8B8A8_UNORM ) {
                return surface_format;
            }
        }

        // Return the first format from the list
        return surface_formats[0];
    }

    VkExtent2D GetSwapChainExtent(VkSurfaceCapabilitiesKHR& surface_capabilities)
    {
        // Special value of surface extent is width == height == -1
        // If this is so we define the size by ourselves but it must fit within defined confines
        if( surface_capabilities.currentExtent.width == uint32_t(-1)) {
            VkExtent2D swap_chain_extent = { 800, 800 };
            if( swap_chain_extent.width < surface_capabilities.minImageExtent.width ) {
                swap_chain_extent.width = surface_capabilities.minImageExtent.width;
            }
            if( swap_chain_extent.height < surface_capabilities.minImageExtent.height ) {
                swap_chain_extent.height = surface_capabilities.minImageExtent.height;
            }
            if( swap_chain_extent.width > surface_capabilities.maxImageExtent.width ) {
                swap_chain_extent.width = surface_capabilities.maxImageExtent.width;
            }
            if( swap_chain_extent.height > surface_capabilities.maxImageExtent.height ) {
                swap_chain_extent.height = surface_capabilities.maxImageExtent.height;
            }
            return swap_chain_extent;
        }

        // Most of the cases we define size of the swap_chain images equal to current window's size
        return surface_capabilities.currentExtent;
    }

    VkImageUsageFlags GetSwapChainUsageFlags(VkSurfaceCapabilitiesKHR& surface_capabilities)
    {
        // Color attachment flag must always be supported
        // We can define other usage flags but we always need to check if they are supported
        if( surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        Assert(false, "VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT image usage is not supported by the swap chain!");
        return static_cast<VkImageUsageFlags>(-1);
    }

    VkSurfaceTransformFlagBitsKHR GetSwapChainTransform(VkSurfaceCapabilitiesKHR& surface_capabilities)
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

    VkPresentModeKHR GetSwapChainPresentMode(std::vector<VkPresentModeKHR>& present_modes)
    {
        // FIFO present mode is always available
        // MAILBOX is the lowest latency V-Sync enabled mode (something like triple-buffering) so use it if available
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
        Assert(false, "FIFO present mode is not supported by the swap chain!");
        return static_cast<VkPresentModeKHR>(-1);
    }
}

using namespace vk;

void create_vulkan_swapchain(ReaperRoot& root, const VulkanBackend& backend, const SwapchainDescriptor& swapchainDesc, PresentationInfo& presentInfo)
{
    static_cast<void>(root); // TODO FIXME
    static_cast<void>(swapchainDesc); // TODO FIXME
    VkSurfaceCapabilitiesKHR surface_capabilities;
    Assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(backend.physicalDevice, presentInfo.surface, &surface_capabilities) == VK_SUCCESS);

    uint32_t formats_count;
    Assert(vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, presentInfo.surface, &formats_count, nullptr) == VK_SUCCESS);
    Assert(formats_count > 0);

    std::vector<VkSurfaceFormatKHR> surface_formats(formats_count);
    Assert(vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, presentInfo.surface, &formats_count, &surface_formats[0]) == VK_SUCCESS);

    uint32_t present_modes_count;
    Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physicalDevice, presentInfo.surface, &present_modes_count, nullptr ) == VK_SUCCESS);
    Assert(present_modes_count > 0);

    std::vector<VkPresentModeKHR> present_modes(present_modes_count);
    Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physicalDevice, presentInfo.surface, &present_modes_count, &present_modes[0]) == VK_SUCCESS);

    // Set of images defined in a swap chain may not always be available for application to render to:
    // One may be displayed and one may wait in a queue to be presented
    // If application wants to use more images at the same time it must ask for more images
    uint32_t image_count = surface_capabilities.minImageCount + 1;
    if ((surface_capabilities.maxImageCount > 0) && (image_count > surface_capabilities.maxImageCount))
        image_count = surface_capabilities.maxImageCount;

    uint32_t                      desired_number_of_images = image_count;
    VkSurfaceFormatKHR            desired_format = GetSwapChainFormat(surface_formats);
    VkExtent2D                    desired_extent = GetSwapChainExtent(surface_capabilities);
    VkImageUsageFlags             desired_usage = GetSwapChainUsageFlags(surface_capabilities);
    VkSurfaceTransformFlagBitsKHR desired_transform = GetSwapChainTransform(surface_capabilities);
    VkPresentModeKHR              desired_present_mode = GetSwapChainPresentMode(present_modes);

    Assert(static_cast<int>(desired_usage) != -1);
    Assert(static_cast<int>(desired_present_mode) != -1);

    VkSwapchainCreateInfoKHR swap_chain_create_info = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,  // VkStructureType                sType
        nullptr,                                      // const void                    *pNext
        0,                                            // VkSwapchainCreateFlagsKHR      flags
        presentInfo.surface,                  // VkSurfaceKHR                   surface
        desired_number_of_images,                     // uint32_t                       minImageCount
        desired_format.format,                        // VkFormat                       imageFormat
        desired_format.colorSpace,                    // VkColorSpaceKHR                imageColorSpace
        desired_extent,                               // VkExtent2D                     imageExtent
        1,                                            // uint32_t                       imageArrayLayers
        desired_usage,                                // VkImageUsageFlags              imageUsage
        VK_SHARING_MODE_EXCLUSIVE,                    // VkSharingMode                  imageSharingMode
        0,                                            // uint32_t                       queueFamilyIndexCount
        nullptr,                                      // const uint32_t                *pQueueFamilyIndices
        desired_transform,                            // VkSurfaceTransformFlagBitsKHR  preTransform
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,            // VkCompositeAlphaFlagBitsKHR    compositeAlpha
        desired_present_mode,                         // VkPresentModeKHR               presentMode
        VK_TRUE,                                      // VkBool32                       clipped
        VK_NULL_HANDLE                                // VkSwapchainKHR                 oldSwapchain
    };

    Assert(vkCreateSwapchainKHR(backend.device, &swap_chain_create_info, nullptr, &presentInfo.swapchain) == VK_SUCCESS);
    Assert(vkGetSwapchainImagesKHR(backend.device, presentInfo.swapchain, &presentInfo.imageCount, nullptr) == VK_SUCCESS);
    Assert(presentInfo.imageCount > 0);

    presentInfo.format = desired_format.format;
    presentInfo.images.resize(presentInfo.imageCount);
    Assert(vkGetSwapchainImagesKHR(backend.device, presentInfo.swapchain, &presentInfo.imageCount, &presentInfo.images[0]) == VK_SUCCESS);
}

void destroy_vulkan_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    static_cast<void>(root); // TODO FIXME
    for (size_t i = 0; i < presentInfo.framebuffers.size(); ++i)
    {
        vkDestroyImageView(backend.device, presentInfo.imageViews[i], nullptr);
        vkDestroyFramebuffer(backend.device, presentInfo.framebuffers[i], nullptr);
    }
    presentInfo.imageViews.clear();
    presentInfo.framebuffers.clear();

    vkDestroySwapchainKHR(backend.device, presentInfo.swapchain, nullptr);
    presentInfo.swapchain = VK_NULL_HANDLE;
}

