////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2018 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

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
    VkImageUsageFlags             usageFlags;
    VkSurfaceTransformFlagBitsKHR transform;

    VkSwapchainKHR             swapchain;
    VkSemaphore                imageAvailableSemaphore;
    VkSemaphore                renderingFinishedSemaphore;
    std::vector<VkImage>       images;
    std::vector<VkImageView>   imageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass               renderPass;
};

struct PhysicalDeviceInfo
{
    uint32_t                         graphicsQueueIndex;
    uint32_t                         presentQueueIndex;
    VkPhysicalDeviceMemoryProperties memory;
};

struct DeviceInfo
{
    // These can point to the same object!
    VkQueue graphicsQueue;
    VkQueue presentQueue;
};

struct REAPER_RENDERER_API VulkanBackend
{
    LibHandle  vulkanLib;
    VkInstance instance;

    VkPhysicalDevice   physicalDevice;
    PhysicalDeviceInfo physicalDeviceInfo;

    VkDevice   device;
    DeviceInfo deviceInfo;

    PresentationInfo presentInfo;

    VkDebugReportCallbackEXT debugCallback;

    VulkanBackend();
};

void create_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& renderer);
void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& renderer);
} // namespace Reaper
