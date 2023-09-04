////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "PhysicalDevice.h"

#include "core/DynamicLibrary.h"
#include "renderer/Renderer.h"
#include "renderer/texture/GPUTextureProperties.h"

#include "vulkan_loader/Vulkan.h"

#include "allocator/AMDVulkanMemoryAllocator.h"

#include <vector>

namespace Reaper
{
struct PresentationInfo
{
    VkSurfaceKHR surface;

    bool queue_swapchain_transition = false;

    // Split this in another struct
    VkSurfaceCapabilitiesKHR      surface_caps;
    VkSurfaceFormatKHR            surface_format;
    VkFormat                      view_format;
    u32                           image_count;
    VkPresentModeKHR              present_mode;
    VkExtent2D                    surface_extent;
    VkImageUsageFlags             swapchain_usage_flags;
    VkSurfaceTransformFlagBitsKHR surface_transform;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    std::vector<VkImage>     images;
    std::vector<VkImageView> imageViews;
};

struct BackendResources;

struct REAPER_RENDERER_API VulkanBackend
{
    LibHandle  vulkanLib = nullptr;
    VkInstance instance = VK_NULL_HANDLE;

    PhysicalDeviceInfo physical_device;

    VkDevice device = VK_NULL_HANDLE;

    // NOTE: These can point to the same object!
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;

    VkDescriptorPool global_descriptor_pool = VK_NULL_HANDLE;

    VmaAllocator vma_instance;

    PresentationInfo presentInfo = {};

    VkExtent2D render_extent;

    // Timeline semaphores aren't supported for present operations at the time AFAIK.
    VkSemaphore semaphore_swapchain_image_available = VK_NULL_HANDLE;
    VkSemaphore semaphore_rendering_finished = VK_NULL_HANDLE;

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    VkExtent2D new_swapchain_extent = {0, 0};

    struct Options
    {
        bool freeze_meshlet_culling = false; // FIXME using the framegraph we can't have persistent resources yet
        bool enable_debug_tile_lighting = true;
    } options;

    BackendResources* resources = nullptr;
};

void create_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend);
void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend);

void set_backend_render_resolution(VulkanBackend& backend);
} // namespace Reaper
