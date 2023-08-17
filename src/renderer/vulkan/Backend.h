////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "allocator/AMDVulkanMemoryAllocator.h"

#include <vulkan_loader/Vulkan.h>

#include "core/DynamicLibrary.h"
#include "renderer/Renderer.h"
#include "renderer/texture/GPUTextureProperties.h"

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

struct PhysicalDeviceInfo
{
    uint32_t                         graphicsQueueFamilyIndex;
    uint32_t                         presentQueueFamilyIndex;
    VkPhysicalDeviceMemoryProperties memory;
    u32                              subgroup_size;
};

struct DeviceInfo
{
    // These can point to the same object!
    VkQueue graphicsQueue;
    VkQueue presentQueue;
};

struct BackendResources;

struct REAPER_RENDERER_API VulkanBackend
{
    LibHandle  vulkanLib = nullptr;
    VkInstance instance = VK_NULL_HANDLE;

    VkPhysicalDevice           physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    PhysicalDeviceInfo         physicalDeviceInfo = {0, 0, {}, 0};

    VkDevice   device = VK_NULL_HANDLE;
    DeviceInfo deviceInfo = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkDescriptorPool global_descriptor_pool = VK_NULL_HANDLE;

    VmaAllocator vma_instance;

    PresentationInfo presentInfo = {};

    // Timeline semaphores aren't supported for present operations at the time AFAIK.
    VkSemaphore semaphore_image_available = VK_NULL_HANDLE;
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

void create_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& renderer);
void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& renderer);
} // namespace Reaper
