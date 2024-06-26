////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "allocator/AMDVulkanMemoryAllocator.h"

#include "api/Vulkan.h"
#include "core/DynamicLibrary.h"
#include "renderer/Renderer.h"
#include "renderer/texture/GPUTextureProperties.h"

namespace Reaper
{
struct PresentationInfo
{
    VkSurfaceKHR surface;

    // Split this in another struct
    VkSurfaceCapabilitiesKHR      surfaceCaps;
    VkSurfaceFormatKHR            surfaceFormat;
    u32                           imageCount;
    VkPresentModeKHR              presentMode;
    VkExtent2D                    surfaceExtent;
    VkImageUsageFlags             swapchainUsageFlags;
    VkSurfaceTransformFlagBitsKHR transform;

    VkSwapchainKHR swapchain;

    // TODO Vulkan 1.3 might allow timeline semaphores soon
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderingFinishedSemaphore;

    std::vector<VkImage>     images;
    std::vector<VkImageView> imageViews;
};

struct PhysicalDeviceInfo
{
    uint32_t                         graphicsQueueFamilyIndex;
    uint32_t                         presentQueueFamilyIndex;
    VkPhysicalDeviceMemoryProperties memory;
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
    LibHandle  vulkanLib;
    VkInstance instance;

    VkPhysicalDevice           physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    PhysicalDeviceInfo         physicalDeviceInfo;

    VkDevice   device;
    DeviceInfo deviceInfo;

    VkDescriptorPool global_descriptor_pool;

    VmaAllocator vma_instance;

    PresentationInfo presentInfo;

    VkDebugUtilsMessengerEXT debugMessenger;

    bool       mustTransitionSwapchain;
    VkExtent2D new_swapchain_extent;

    struct Options
    {
        bool freeze_culling;
        bool use_compacted_draw;
    } options;

    BackendResources* resources;

public:
    VulkanBackend();
};

void create_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& renderer);
void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& renderer);
} // namespace Reaper
