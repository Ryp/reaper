////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Swapchain.h"

#include "Backend.h"
#include "Image.h"

#include "api/AssertHelper.h"
#include "api/VulkanStringConversion.h"
#include <vulkan_loader/Vulkan.h>

#include "common/Log.h"

#include "core/Assert.h"
#include "profiling/Scope.h"

#include <algorithm>
#include <span>

namespace
{
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
    // currentExtent is the current width and height of the surface, or the special
    // value (0xFFFFFFFF, 0xFFFFFFFF) indicating that the surface size will be determined by the
    // extent of a swapchain targeting the surface.
    const u32 special = 0xFFFFFFFF;

    if (surface_capabilities.currentExtent.width == special && surface_capabilities.currentExtent.height == special)
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

VkPresentModeKHR vulkan_swapchain_choose_present_mode(std::span<const VkPresentModeKHR> present_modes)
{
    // Prefer MAILBOX over FIFO
    for (VkPresentModeKHR present_mode : present_modes)
    {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}
} // namespace

namespace Reaper
{
namespace
{
    ColorSpace get_color_space(VkSurfaceFormatKHR surface_format)
    {
        switch (surface_format.colorSpace)
        {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: // Allows negative values
            return ColorSpace::sRGB;
        case VK_COLOR_SPACE_BT709_LINEAR_EXT:
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
            return ColorSpace::Rec709;
        case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:
            return ColorSpace::DisplayP3;
        case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
        case VK_COLOR_SPACE_HDR10_ST2084_EXT:
        case VK_COLOR_SPACE_HDR10_HLG_EXT:
            return ColorSpace::Rec2020;
        case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD:
            switch (surface_format.format)
            {
            // See https://github.com/GPUOpen-LibrariesAndSDKs/FreesyncPremiumProSample/blob/master/readme.md
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
                return ColorSpace::sRGB; // FIXME AGSDisplaySettings::Mode::Mode_Freesync2_Gamma22
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return ColorSpace::sRGB; // FIXME AGSDisplaySettings::Mode::Mode_Freesync2_scRGB
            default:
                break;
            }
        default:
            break;
        }

        AssertUnreachable();
        return ColorSpace::Unknown;
    }

    SwapchainFormat create_surface_format(VkSurfaceFormatKHR surface_format)
    {
        SwapchainFormat sf;
        sf.vk_format = surface_format.format;
        sf.vk_color_space = surface_format.colorSpace;
        sf.vk_view_format = surface_format.format;

        sf.color_space = get_color_space(surface_format);

        switch (surface_format.colorSpace)
        {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
            sf.transfer_function = TransferFunction::sRGB;
            sf.is_hdr = false;
            break;
        case VK_COLOR_SPACE_BT709_LINEAR_EXT:
            sf.transfer_function = TransferFunction::Linear;
            sf.is_hdr = false;
            break;
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
            sf.transfer_function = TransferFunction::Rec709;
            sf.is_hdr = false;
            break;
        case VK_COLOR_SPACE_HDR10_ST2084_EXT:
            sf.transfer_function = TransferFunction::PQ;
            sf.is_hdr = true;
            break;
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
            sf.transfer_function = TransferFunction::scRGB_Windows;
            sf.is_hdr = true;
            break;
        default:
            break;
        }

        return sf;
    }

    // Welcome to the fun world of choosing a swapchain format for Vulkan, but especially - HDR formats!
    // At the time of writing this there's several caveats:
    //
    // * Win11 SDR Mode + NVIDIA 551.81 + RGB10A2 + PQ
    // In this specific combination, you have an HDR capable screen but windows is set to SDR mode.
    // If you choose a RGB10+PQ swapchain, windows WON'T be switched to HDR mode and you'll get horrible banding if your
    // display link was set to 8bits per channel (most users). This happens BOTH in borderless fullscreen as well as
    // windowed. Note that this doesn't happen with scRGB swapchains, in that case the driver correctly switches windows
    // in HDR mode.
    // https://forums.developer.nvidia.com/t/bug-driver-switches-display-to-hdr-from-vulkan-pq-swapchain-but-keeps-8-bit-display-link-when-windows-is-in-sdr-mode/287197
    //
    // * Win11 + AMD
    // AMD doesn't report HDR formats when Windows is not in HDR mode itself. Other than that it behaves consistently.
    //
    // * Linux and HDR
    // Support is very limited right now, but efforts are underway. More patience needed.
    SwapchainFormat choose_swapchain_format(ReaperRoot& root, std::span<const VkSurfaceFormat2KHR> formats,
                                            VkSurfaceFormatKHR preferred_format)
    {
        std::vector<SwapchainFormat> supported_formats;

        log_debug(root, "vulkan: swapchain supports {} formats", formats.size());
        for (const auto& format : formats)
        {
            log_debug(root, "- format = {}, colorspace = {}", vk_to_string(format.surfaceFormat.format),
                      vk_to_string(format.surfaceFormat.colorSpace));

            const SwapchainFormat swapchain_format = create_surface_format(format.surfaceFormat);

            // If we had a preferred format and found it, just return now
            if (format.surfaceFormat.format == preferred_format.format
                && format.surfaceFormat.colorSpace == preferred_format.colorSpace)
            {
                return swapchain_format;
            }

            // Some formats combinations are not handled, so we're filtering them out here
            if (swapchain_format.color_space != ColorSpace::Unknown
                && swapchain_format.transfer_function != TransferFunction::Unknown)
            {
                supported_formats.emplace_back(swapchain_format);
            }
        }

        Assert(!supported_formats.empty());

        // We sort formats with a heuristic that tries to make sense.
        auto comparison_less_lambda = [](SwapchainFormat a, SwapchainFormat b) -> bool {
            if (a.is_hdr == b.is_hdr)
            {
                if (a.transfer_function == b.transfer_function)
                {
                    // Avoid 64-bit RTs if possible
                    // NOTE: To be more precise we should have a function that returns the storage cost instead of just
                    // handling one format.
                    return a.vk_format != VK_FORMAT_R16G16B16A16_SFLOAT && b.vk_format == VK_FORMAT_R16G16B16A16_SFLOAT;
                }

                // Prefer PQ-encoded swapchains
                return a.transfer_function == TransferFunction::PQ && b.transfer_function != TransferFunction::PQ;
            }

            // Prefer HDR swapchains
            return a.is_hdr && !b.is_hdr;
        };

        std::sort(supported_formats.begin(), supported_formats.end(), comparison_less_lambda);

        log_debug(root, "vulkan: choosing {} formats from this list", supported_formats.size());
        for (const auto& format : supported_formats)
        {
            log_debug(root, "- format = {}, colorspace = {}", vk_to_string(format.vk_format),
                      vk_to_string(format.vk_color_space));
        }

        SwapchainFormat swapchain_format = supported_formats.front();

        log_debug(root, "vulkan: selecting swapchain format = {}, colorspace = {}",
                  vk_to_string(swapchain_format.vk_format), vk_to_string(swapchain_format.vk_color_space));
        log_debug(root, "vulkan: selecting swapchain view format = {}", vk_to_string(swapchain_format.vk_view_format));

        return swapchain_format;
    }
} // namespace

void configure_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend,
                                   const SwapchainDescriptor& swapchainDesc, PresentationInfo& presentInfo)
{
    log_debug(root, "vulkan: configuring wm swapchain");

    VkPhysicalDeviceSurfaceInfo2KHR surface_info_2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .pNext = nullptr,
        .surface = presentInfo.surface,
    };

    VkSurfaceCapabilities2KHR surface_caps_2 = {};
    surface_caps_2.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

    AssertVk(
        vkGetPhysicalDeviceSurfaceCapabilities2KHR(backend.physical_device.handle, &surface_info_2, &surface_caps_2));

    presentInfo.surface_caps = surface_caps_2.surfaceCapabilities;
    const VkSurfaceCapabilitiesKHR& surface_caps = surface_caps_2.surfaceCapabilities;

    uint32_t formats_count;
    AssertVk(vkGetPhysicalDeviceSurfaceFormats2KHR(backend.physical_device.handle, &surface_info_2, &formats_count,
                                                   nullptr));
    Assert(formats_count > 0);

    std::vector<VkSurfaceFormat2KHR> surface_formats(
        formats_count,
        VkSurfaceFormat2KHR{.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, .pNext = nullptr, .surfaceFormat = {}});

    AssertVk(vkGetPhysicalDeviceSurfaceFormats2KHR(backend.physical_device.handle, &surface_info_2, &formats_count,
                                                   surface_formats.data()));

    // Choose surface format
    presentInfo.swapchain_format = choose_swapchain_format(root, surface_formats, swapchainDesc.preferredFormat);

    if (presentInfo.swapchain_format.transfer_function == TransferFunction::sRGB)
    {
        // In the case of sRGB, the EOTF is usually done with the texture view already, so no need to apply it in
        // the shader.
        presentInfo.swapchain_format.transfer_function = TransferFunction::Linear;

        // Overriding the view format lets us get the sRGB eotf for free.
        // This needs VK_KHR_swapchain_mutable_format to work
        switch (presentInfo.swapchain_format.vk_format)
        {
        case VK_FORMAT_B8G8R8A8_UNORM:
            presentInfo.swapchain_format.vk_view_format = VK_FORMAT_B8G8R8A8_SRGB;
            break;
        case VK_FORMAT_R8G8B8A8_UNORM:
            presentInfo.swapchain_format.vk_view_format = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            presentInfo.swapchain_format.vk_view_format = VK_FORMAT_A8B8G8R8_SRGB_PACK32;
            break;
        default:
            break;
        }
    }

    // Image count
    uint32_t image_count = swapchainDesc.preferredImageCount;
    {
        log_debug(root, "vulkan: swapchain image count support: min = {}, max = {}", surface_caps.minImageCount,
                  surface_caps.maxImageCount);

        if (image_count < surface_caps.minImageCount)
            image_count = surface_caps.minImageCount;

        // Max count might be zero when uncapped
        if (image_count > surface_caps.maxImageCount && surface_caps.maxImageCount > 0)
            image_count = surface_caps.maxImageCount;

        if (image_count != swapchainDesc.preferredImageCount)
        {
            log_warning(root, "vulkan: swapchain image count {} is unsupported. falling back to {}",
                        swapchainDesc.preferredImageCount, image_count);
        }

        Assert(image_count >= 2, "Swapchain should support at least double-buffering");
        Assert(image_count <= 5, "Swapchain image count too large");

        presentInfo.image_count = image_count;
    }

    {
        uint32_t presentModeCount;
        AssertVk(vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physical_device.handle, presentInfo.surface,
                                                           &presentModeCount, nullptr));
        Assert(presentModeCount > 0);

        std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
        AssertVk(vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physical_device.handle, presentInfo.surface,
                                                           &presentModeCount, availablePresentModes.data()));

        log_debug(root, "vulkan: swapchain supports {} present modes", presentModeCount);
        for (auto& mode : availablePresentModes)
            log_debug(root, "- {}", vk_to_string(mode));

        presentInfo.present_mode = vulkan_swapchain_choose_present_mode(availablePresentModes);
    }

    {
        VkExtent2D extent = vulkan_swapchain_choose_extent(surface_caps, swapchainDesc.preferredExtent);

        if (extent.width != swapchainDesc.preferredExtent.width
            || extent.height != swapchainDesc.preferredExtent.height)
        {
            log_warning(root, "vulkan: swapchain extent {}x{} is not supported", swapchainDesc.preferredExtent.width,
                        swapchainDesc.preferredExtent.height);
            log_warning(root, "- falling back to {}x{}", extent.width, extent.height);
        }
        presentInfo.surface_extent = extent;
    }

    // Usage flags
    // NOTE: Some flags might be added at runtime by external forces (driver, layers?)
    // and might trigger a validation warning down the line.
    // I suspect this is primarily done for frame capture purposes, like adding TRANSFER_SRC to allow copies.
    // It should be safe to ignore in this case.
    presentInfo.swapchain_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    Assert((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0, "Vulkan API error");

    // Transform
    presentInfo.surface_transform = vulkan_swapchain_choose_transform(surface_caps);
}

void create_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    REAPER_PROFILE_SCOPE_FUNC();
    log_debug(root, "vulkan: creating wm swapchain");

    const std::vector<VkFormat> view_formats = {
        presentInfo.swapchain_format.vk_format,      // Usual format if the swapchain was immutable
        presentInfo.swapchain_format.vk_view_format, // Format to use the mutable feature with
    };

    VkImageFormatListCreateInfo format_list = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
        .pNext = nullptr,
        .viewFormatCount = static_cast<u32>(view_formats.size()),
        .pViewFormats = view_formats.data(),
    };

    VkSwapchainCreateInfoKHR swap_chain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = &format_list,
        .flags = VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR,
        .surface = presentInfo.surface,
        .minImageCount = presentInfo.image_count,
        .imageFormat = presentInfo.swapchain_format.vk_format,
        .imageColorSpace = presentInfo.swapchain_format.vk_color_space,
        .imageExtent = presentInfo.surface_extent,
        .imageArrayLayers = 1,
        .imageUsage = presentInfo.swapchain_usage_flags,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = presentInfo.surface_transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentInfo.present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = presentInfo.swapchain,
    };

    AssertVk(vkCreateSwapchainKHR(backend.device, &swap_chain_create_info, nullptr, &presentInfo.swapchain));

    log_info(root, "vulkan: swapchain created with format = {}, colorspace = {}",
             vk_to_string(presentInfo.swapchain_format.vk_format),
             vk_to_string(presentInfo.swapchain_format.vk_color_space));

    u32 actualImageCount = 0;
    AssertVk(vkGetSwapchainImagesKHR(backend.device, presentInfo.swapchain, &actualImageCount, nullptr));

    Assert(actualImageCount > 0);
    Assert(actualImageCount >= presentInfo.image_count, "Invalid swapchain image count returned");

    presentInfo.images.resize(actualImageCount);
    AssertVk(
        vkGetSwapchainImagesKHR(backend.device, presentInfo.swapchain, &actualImageCount, presentInfo.images.data()));

    if (actualImageCount != presentInfo.image_count)
    {
        log_warning(root, "vulkan: {} swapchain images were asked but we got {}", presentInfo.image_count,
                    actualImageCount);
    }

    // Since we don't control which swapchain image index we'll get when acquiring one,
    // we have to use the full amount of swapchain we get back.
    presentInfo.image_count = actualImageCount;

    create_swapchain_views(backend, presentInfo);

    presentInfo.queue_swapchain_transition = true;
}

void destroy_vulkan_wm_swapchain(ReaperRoot& root, const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    REAPER_PROFILE_SCOPE_FUNC();
    log_debug(root, "vulkan: destroying wm swapchain");

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
    AssertVk(vkDeviceWaitIdle(backend.device));

    destroy_swapchain_views(backend, presentInfo);

    // Reconfigure even if we know most of what we expect/need
    SwapchainDescriptor swapchainDesc;
    swapchainDesc.preferredImageCount = presentInfo.image_count;
    swapchainDesc.preferredFormat = {presentInfo.swapchain_format.vk_format,
                                     presentInfo.swapchain_format.vk_color_space};
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
    const size_t image_count = presentInfo.image_count;

    Assert(image_count > 0);

    presentInfo.imageViews.resize(image_count);

    for (size_t i = 0; i < image_count; ++i)
    {
        const GPUTextureView view = {
            .type = GPUTextureViewType::Tex2D,
            .format = VulkanToPixelFormat(presentInfo.swapchain_format.vk_view_format),
            .subresource = default_texture_subresource_one_color_mip(),
        };

        presentInfo.imageViews[i] = create_image_view(backend.device, presentInfo.images[i], view);
    }
}

void destroy_swapchain_views(const VulkanBackend& backend, PresentationInfo& presentInfo)
{
    for (size_t i = 0; i < presentInfo.imageViews.size(); i++)
        vkDestroyImageView(backend.device, presentInfo.imageViews[i], nullptr);

    presentInfo.imageViews.clear();
}
} // namespace Reaper
