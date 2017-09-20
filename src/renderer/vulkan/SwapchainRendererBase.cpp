////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// TODO use Pimpl to prevent name clashes between Xlib and fmt and move this include forward
#include "common/Log.h"

#include "core/Profile.h"

#include "Swapchain.h"
#include "SwapchainRendererBase.h"

#include <cstring>
#include <iostream>

#include "PresentationSurface.h"
#include "renderer/window/Window.h"

using namespace vk;

void vulkan_instance_check_extensions(const std::vector<const char*>& extensions);
void vulkan_instance_check_layers(const std::vector<const char*>& layers);

void vulkan_device_check_extensions(const std::vector<const char*>& extensions, VkPhysicalDevice physicalDevice);

void vulkan_setup_debug_callback(ReaperRoot& root, VulkanBackend& renderer);
void vulkan_destroy_debug_callback(VulkanBackend& renderer);

bool vulkan_check_physical_device(IWindow*                        window,
                                  VkPhysicalDevice                physical_device,
                                  VkSurfaceKHR                    presentationSurface,
                                  const std::vector<const char*>& extensions,
                                  uint32_t&                       queue_family_index,
                                  uint32_t&                       selected_present_queue_family_index);
void vulkan_choose_physical_device(ReaperRoot& root, VulkanBackend& backend, PhysicalDeviceInfo& physicalDeviceInfo);
void vulkan_create_logical_device(ReaperRoot& root, VulkanBackend& backend);

namespace
{
    bool checkExtensionAvailability(const char*                               extension_name,
                                    const std::vector<VkExtensionProperties>& available_extensions)
    {
        for (size_t i = 0; i < available_extensions.size(); ++i)
        {
            if (std::strcmp(available_extensions[i].extensionName, extension_name) == 0)
                return true;
        }
        return false;
    }

    bool checkLayerAvailability(const char* layer_name, const std::vector<VkLayerProperties>& available_layers)
    {
        for (size_t i = 0; i < available_layers.size(); ++i)
        {
            if (std::strcmp(available_layers[i].layerName, layer_name) == 0)
                return true;
        }
        return false;
    }

    bool checkPhysicalDeviceProperties(VkPhysicalDevice physical_device,
                                       VkSurfaceKHR     presentationSurface,
                                       uint32_t&        queue_family_index,
                                       uint32_t&        selected_present_queue_family_index)
    {
        uint32_t extensions_count = 0;
        Assert(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensions_count, nullptr)
               == VK_SUCCESS);
        Assert(extensions_count > 0);

        std::vector<VkExtensionProperties> available_extensions(extensions_count);
        Assert(
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensions_count, &available_extensions[0])
            == VK_SUCCESS);

        std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        Assert(extensions.size() == 1);

        for (size_t i = 0; i < extensions.size(); ++i)
            Assert(checkExtensionAvailability(extensions[i], available_extensions));

        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures   device_features;

        vkGetPhysicalDeviceProperties(physical_device, &device_properties);
        vkGetPhysicalDeviceFeatures(physical_device, &device_features);

        uint32_t major_version = VK_VERSION_MAJOR(device_properties.apiVersion);
        //     uint32_t minor_version = VK_VERSION_MINOR(device_properties.apiVersion);
        //     uint32_t patch_version = VK_VERSION_PATCH(device_properties.apiVersion);

        Assert(major_version >= 1);
        Assert(device_properties.limits.maxImageDimension2D >= 4096);
        Assert(device_features.shaderClipDistance == VK_TRUE); // This is just checked, not enabled

        uint32_t queue_families_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

        Assert(queue_families_count > 0, "device doesn't have any queue families");
        if (queue_families_count == 0)
            return false;

        std::vector<VkQueueFamilyProperties> queue_family_properties(queue_families_count);
        std::vector<VkBool32>                queue_present_support(queue_families_count);

        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, &queue_family_properties[0]);

        uint32_t graphics_queue_family_index = UINT32_MAX;
        uint32_t present_queue_family_index = UINT32_MAX;

        for (uint32_t i = 0; i < queue_families_count; ++i)
        {
            Assert(
                vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, presentationSurface, &queue_present_support[i])
                == VK_SUCCESS);

            if ((queue_family_properties[i].queueCount > 0)
                && (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                // Select first queue that supports graphics
                if (graphics_queue_family_index == UINT32_MAX)
                {
                    graphics_queue_family_index = i;
                }

                // If there is queue that supports both graphics and present - prefer it
                if (queue_present_support[i])
                {
                    queue_family_index = i;
                    selected_present_queue_family_index = i;
                    return true;
                }
            }
        }

        // We don't have queue that supports both graphics and present so we have to use separate queues
        for (uint32_t i = 0; i < queue_families_count; ++i)
        {
            if (queue_present_support[i] == VK_TRUE)
            {
                present_queue_family_index = i;
                break;
            }
        }

        Assert(graphics_queue_family_index != UINT32_MAX);
        Assert(present_queue_family_index != UINT32_MAX);

        queue_family_index = graphics_queue_family_index;
        selected_present_queue_family_index = present_queue_family_index;
        return true;
    }

    VkBool32 debugReportCallbackMf(VkDebugReportFlagsEXT /*flags*/,
                                   VkDebugReportObjectTypeEXT /*objectType*/,
                                   uint64_t /*object*/,
                                   size_t /*location*/,
                                   int32_t /*messageCode*/,
                                   const char* /*pLayerPrefix*/,
                                   const char* pMessage,
                                   void* /*pUserData*/)
    {
        Assert(false, pMessage);
        // std::cerr << pMessage << std::endl;
        return VK_FALSE;
    }
}

void SwapchainRendererBase::startup(IWindow* window)
{
    _vulkanLib = dynlib::load(REAPER_VK_LIB_NAME);

    _instance = VK_NULL_HANDLE;
    _physicalDevice = VK_NULL_HANDLE;
    _imageAvailableSemaphore = VK_NULL_HANDLE;
    _renderingFinishedSemaphore = VK_NULL_HANDLE;
    _presentationSurface = VK_NULL_HANDLE;
    _presentationQueue = VK_NULL_HANDLE;
    _graphicsQueue = VK_NULL_HANDLE;
    _swapChain = VK_NULL_HANDLE;
    _device = VK_NULL_HANDLE;

    _debugCallback = VK_NULL_HANDLE;

    _mainRenderPass = VK_NULL_HANDLE;

    _gfxCmdPool = VK_NULL_HANDLE;
    _gfxCmdBuffers.clear();
    _gfxQueueIndex = UINT32_MAX;
    _prsQueueIndex = UINT32_MAX;

#define REAPER_VK_EXPORTED_FUNCTION(func)                        \
    {                                                            \
        func = (PFN_##func)dynlib::getSymbol(_vulkanLib, #func); \
    }
#include "renderer/vulkan/api/VulkanSymbolHelper.inl"

#define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func)                               \
    {                                                                       \
        func = (PFN_##func)vkGetInstanceProcAddr(nullptr, #func);           \
        Assert(func != nullptr, "could not load global level vk function"); \
    }
#include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    uint32_t extensions_count = 0;
    Assert(vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, nullptr) == VK_SUCCESS);
    Assert(extensions_count > 0);

    std::vector<VkExtensionProperties> available_extensions(extensions_count);
    Assert(vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, &available_extensions[0]) == VK_SUCCESS);

    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME, REAPER_VK_SWAPCHAIN_EXTENSION_NAME};

    Assert(extensions.size() == 2);

#if defined(REAPER_DEBUG)
    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

    for (size_t i = 0; i < extensions.size(); ++i)
        Assert(checkExtensionAvailability(extensions[i], available_extensions));

    VkApplicationInfo application_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType            sType
        nullptr,                            // const void                *pNext
        "Tower Defense FTW",                // const char                *pApplicationName
        VK_MAKE_VERSION(1, 0, 0),           // uint32_t                   applicationVersion
        "Reaper",                           // const char                *pEngineName
        VK_MAKE_VERSION(1, 0, 0),           // uint32_t                   engineVersion
        REAPER_VK_API_VERSION               // uint32_t                   apiVersion
    };

    std::vector<const char*> enabledInstanceLayers;

    uint32_t layers_count = 0;
    Assert(vkEnumerateInstanceLayerProperties(&layers_count, nullptr) == VK_SUCCESS);
    Assert(layers_count > 0);

    std::vector<VkLayerProperties> available_layers(layers_count);
    Assert(vkEnumerateInstanceLayerProperties(&layers_count, &available_layers[0]) == VK_SUCCESS);

#if defined(REAPER_DEBUG)
    enabledInstanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

    for (size_t i = 0; i < enabledInstanceLayers.size(); ++i)
        Assert(checkLayerAvailability(enabledInstanceLayers[i], available_layers));

    VkInstanceCreateInfo instance_create_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,              // VkStructureType            sType
        nullptr,                                             // const void*                pNext
        0,                                                   // VkInstanceCreateFlags      flags
        &application_info,                                   // const VkApplicationInfo   *pApplicationInfo
        static_cast<uint32_t>(enabledInstanceLayers.size()), // uint32_t                   enabledLayerCount
        &enabledInstanceLayers[0],                           // const char * const        *ppEnabledLayerNames
        static_cast<uint32_t>(extensions.size()),            // uint32_t                   enabledExtensionCount
        &extensions[0]                                       // const char * const        *ppEnabledExtensionNames
    };

    Assert(vkCreateInstance(&instance_create_info, nullptr, &_instance) == VK_SUCCESS, "cannot create Vulkan instance");

#define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func)                               \
    {                                                                         \
        func = (PFN_##func)vkGetInstanceProcAddr(_instance, #func);           \
        Assert(func != nullptr, "could not load instance level vk function"); \
    }
#include "renderer/vulkan/api/VulkanSymbolHelper.inl"

#if defined(REAPER_DEBUG)
    /* Setup callback creation information */
    VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
    callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    callbackCreateInfo.pNext = nullptr;
    callbackCreateInfo.flags =
        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    callbackCreateInfo.pfnCallback = &debugReportCallbackMf;
    callbackCreateInfo.pUserData = nullptr;

    Assert(vkCreateDebugReportCallbackEXT(_instance, &callbackCreateInfo, nullptr, &_debugCallback) == VK_SUCCESS);
#endif

    vulkan_create_presentation_surface(_instance, _presentationSurface, window);

    uint32_t deviceNo = 0;

    Assert(vkEnumeratePhysicalDevices(_instance, &deviceNo, nullptr) == VK_SUCCESS);
    Assert(deviceNo > 0);

    std::vector<VkPhysicalDevice> availableDevices(deviceNo);
    Assert(vkEnumeratePhysicalDevices(_instance, &deviceNo, &availableDevices[0]) == VK_SUCCESS,
           "error occurred during physical devices enumeration");

    uint32_t selected_queue_family_index = UINT32_MAX;
    uint32_t selected_present_queue_family_index = UINT32_MAX;

    for (uint32_t i = 0; i < deviceNo; ++i)
    {
        if (checkPhysicalDeviceProperties(availableDevices[i],
                                          _presentationSurface,
                                          selected_queue_family_index,
                                          selected_present_queue_family_index))
        {
            _physicalDevice = availableDevices[i];
            break;
        }
    }
    Assert(_physicalDevice != VK_NULL_HANDLE, "could not select physical device based on the chosen properties");

    _prsQueueIndex = selected_present_queue_family_index;
    _gfxQueueIndex = selected_queue_family_index;

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::vector<float>                   queue_priorities = {1.0f};

    queue_create_infos.push_back({
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     // VkStructureType              sType
        nullptr,                                        // const void                  *pNext
        0,                                              // VkDeviceQueueCreateFlags     flags
        selected_queue_family_index,                    // uint32_t                     queueFamilyIndex
        static_cast<uint32_t>(queue_priorities.size()), // uint32_t                     queueCount
        &queue_priorities[0]                            // const float                 *pQueuePriorities
    });

    if (selected_queue_family_index != selected_present_queue_family_index)
    {
        queue_create_infos.push_back({
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     // VkStructureType              sType
            nullptr,                                        // const void                  *pNext
            0,                                              // VkDeviceQueueCreateFlags     flags
            selected_present_queue_family_index,            // uint32_t                     queueFamilyIndex
            static_cast<uint32_t>(queue_priorities.size()), // uint32_t                     queueCount
            &queue_priorities[0]                            // const float                 *pQueuePriorities
        });
    }

    std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::vector<const char*> device_layers;

#if defined(REAPER_DEBUG)
    device_layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

    VkDeviceCreateInfo device_create_info = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,             // VkStructureType                    sType
        nullptr,                                          // const void                        *pNext
        0,                                                // VkDeviceCreateFlags                flags
        static_cast<uint32_t>(queue_create_infos.size()), // uint32_t                           queueCreateInfoCount
        &queue_create_infos[0],                           // const VkDeviceQueueCreateInfo     *pQueueCreateInfos
        static_cast<uint32_t>(device_layers.size()),      // uint32_t                           enabledLayerCount
        &device_layers[0],                                // const char * const                *ppEnabledLayerNames
        static_cast<uint32_t>(device_extensions.size()),  // uint32_t                           enabledExtensionCount
        &device_extensions[0],                            // const char * const                *ppEnabledExtensionNames
        nullptr                                           // const VkPhysicalDeviceFeatures    *pEnabledFeatures
    };

    Assert(vkCreateDevice(_physicalDevice, &device_create_info, nullptr, &_device) == VK_SUCCESS,
           "could not create Vulkan device");

#define REAPER_VK_DEVICE_LEVEL_FUNCTION(func)                               \
    {                                                                       \
        func = (PFN_##func)vkGetDeviceProcAddr(_device, #func);             \
        Assert(func != nullptr, "could not load device level vk function"); \
    }
#include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    vkGetDeviceQueue(_device, selected_queue_family_index, 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, selected_present_queue_family_index, 0, &_presentationQueue);

    createSemaphores();
    createSwapChain();
    createRenderPass();
    createFramebuffers();
    createCommandBufferPool();
}

void SwapchainRendererBase::shutdown()
{
    Assert(vkDeviceWaitIdle(_device) == VK_SUCCESS);

    vkFreeCommandBuffers(_device, _gfxCmdPool, static_cast<uint32_t>(_gfxCmdBuffers.size()), &_gfxCmdBuffers[0]);
    vkDestroyCommandPool(_device, _gfxCmdPool, nullptr);

    vkDestroyRenderPass(_device, _mainRenderPass, nullptr);

    vkDestroySemaphore(_device, _imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(_device, _renderingFinishedSemaphore, nullptr);

    for (size_t i = 0; i < _framebuffers.size(); ++i)
    {
        vkDestroyImageView(_device, _swapChainImageViews[i], nullptr);
        vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapChain, nullptr);

    vkDestroySurfaceKHR(_instance, _presentationSurface, nullptr);
    vkDestroyDevice(_device, nullptr);

#if defined(REAPER_DEBUG)
    vkDestroyDebugReportCallbackEXT(_instance, _debugCallback, nullptr);
#endif

    vkDestroyInstance(_instance, nullptr);

    dynlib::close(_vulkanLib);
}

void SwapchainRendererBase::createSemaphores()
{
    VkSemaphoreCreateInfo semaphore_create_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, // VkStructureType          sType
        nullptr,                                 // const void*              pNext
        0                                        // VkSemaphoreCreateFlags   flags
    };

    Assert(vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_imageAvailableSemaphore) == VK_SUCCESS);
    Assert(vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_renderingFinishedSemaphore) == VK_SUCCESS);
}

namespace
{
    VkSurfaceFormatKHR GetSwapChainFormat(std::vector<VkSurfaceFormatKHR>& surface_formats)
    {
        // If the list contains only one entry with undefined format
        // it means that there are no preferred surface formats and any can be chosen
        if ((surface_formats.size() == 1) && (surface_formats[0].format == VK_FORMAT_UNDEFINED))
        {
            return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR};
        }

        // Check if list contains most widely used R8 G8 B8 A8 format
        // with nonlinear color space
        for (VkSurfaceFormatKHR& surface_format : surface_formats)
        {
            if (surface_format.format == VK_FORMAT_R8G8B8A8_UNORM)
            {
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
        if (surface_capabilities.currentExtent.width == uint32_t(-1))
        {
            VkExtent2D swap_chain_extent = {800, 800};
            if (swap_chain_extent.width < surface_capabilities.minImageExtent.width)
            {
                swap_chain_extent.width = surface_capabilities.minImageExtent.width;
            }
            if (swap_chain_extent.height < surface_capabilities.minImageExtent.height)
            {
                swap_chain_extent.height = surface_capabilities.minImageExtent.height;
            }
            if (swap_chain_extent.width > surface_capabilities.maxImageExtent.width)
            {
                swap_chain_extent.width = surface_capabilities.maxImageExtent.width;
            }
            if (swap_chain_extent.height > surface_capabilities.maxImageExtent.height)
            {
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
        if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        {
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
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        {
            return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        }
        else
        {
            return surface_capabilities.currentTransform;
        }
    }

    VkPresentModeKHR GetSwapChainPresentMode(std::vector<VkPresentModeKHR>& present_modes)
    {
        // FIFO present mode is always available
        // MAILBOX is the lowest latency V-Sync enabled mode (something like triple-buffering) so use it if available
        for (VkPresentModeKHR& present_mode : present_modes)
        {
            if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return present_mode;
        }
        for (VkPresentModeKHR& present_mode : present_modes)
        {
            if (present_mode == VK_PRESENT_MODE_FIFO_KHR)
                return present_mode;
        }
        Assert(false, "FIFO present mode is not supported by the swap chain!");
        return static_cast<VkPresentModeKHR>(-1);
    }
}

void SwapchainRendererBase::createSwapChain()
{
    VkSurfaceCapabilitiesKHR surface_capabilities;
    Assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _presentationSurface, &surface_capabilities)
           == VK_SUCCESS);

    uint32_t formats_count;
    Assert(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _presentationSurface, &formats_count, nullptr)
           == VK_SUCCESS);
    Assert(formats_count > 0);

    std::vector<VkSurfaceFormatKHR> surface_formats(formats_count);
    Assert(
        vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _presentationSurface, &formats_count, &surface_formats[0])
        == VK_SUCCESS);

    uint32_t present_modes_count;
    Assert(
        vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _presentationSurface, &present_modes_count, nullptr)
        == VK_SUCCESS);
    Assert(present_modes_count > 0);

    std::vector<VkPresentModeKHR> present_modes(present_modes_count);
    Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(
               _physicalDevice, _presentationSurface, &present_modes_count, &present_modes[0])
           == VK_SUCCESS);

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
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, // VkStructureType                sType
        nullptr,                                     // const void                    *pNext
        0,                                           // VkSwapchainCreateFlagsKHR      flags
        _presentationSurface,                        // VkSurfaceKHR                   surface
        desired_number_of_images,                    // uint32_t                       minImageCount
        desired_format.format,                       // VkFormat                       imageFormat
        desired_format.colorSpace,                   // VkColorSpaceKHR                imageColorSpace
        desired_extent,                              // VkExtent2D                     imageExtent
        1,                                           // uint32_t                       imageArrayLayers
        desired_usage,                               // VkImageUsageFlags              imageUsage
        VK_SHARING_MODE_EXCLUSIVE,                   // VkSharingMode                  imageSharingMode
        0,                                           // uint32_t                       queueFamilyIndexCount
        nullptr,                                     // const uint32_t                *pQueueFamilyIndices
        desired_transform,                           // VkSurfaceTransformFlagBitsKHR  preTransform
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,           // VkCompositeAlphaFlagBitsKHR    compositeAlpha
        desired_present_mode,                        // VkPresentModeKHR               presentMode
        VK_TRUE,                                     // VkBool32                       clipped
        VK_NULL_HANDLE                               // VkSwapchainKHR                 oldSwapchain
    };

    Assert(vkCreateSwapchainKHR(_device, &swap_chain_create_info, nullptr, &_swapChain) == VK_SUCCESS);
    Assert(vkGetSwapchainImagesKHR(_device, _swapChain, &_swapChainImageCount, nullptr) == VK_SUCCESS);
    Assert(_swapChainImageCount > 0);

    _swapChainFormat = desired_format.format;
    _swapChainImages.resize(_swapChainImageCount);
    Assert(vkGetSwapchainImagesKHR(_device, _swapChain, &_swapChainImageCount, &_swapChainImages[0]) == VK_SUCCESS);
}

void SwapchainRendererBase::createRenderPass()
{
    VkAttachmentDescription attachment_descriptions[] = {{
        0,                                // VkAttachmentDescriptionFlags   flags
        _swapChainFormat,                 // VkFormat                       format
        VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits          samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,      // VkAttachmentLoadOp             loadOp
        VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp            storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp             stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp            stencilStoreOp
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // VkImageLayout                  initialLayout;
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR   // VkImageLayout                  finalLayout
    }};

    VkAttachmentReference color_attachment_references[] = {{
        0,                                       // uint32_t                       attachment
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                  layout
    }};

    VkSubpassDescription subpass_descriptions[] = {{
        0,                               // VkSubpassDescriptionFlags      flags
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint            pipelineBindPoint
        0,                               // uint32_t                       inputAttachmentCount
        nullptr,                         // const VkAttachmentReference   *pInputAttachments
        1,                               // uint32_t                       colorAttachmentCount
        color_attachment_references,     // const VkAttachmentReference   *pColorAttachments
        nullptr,                         // const VkAttachmentReference   *pResolveAttachments
        nullptr,                         // const VkAttachmentReference   *pDepthStencilAttachment
        0,                               // uint32_t                       preserveAttachmentCount
        nullptr                          // const uint32_t*                pPreserveAttachments
    }};

    VkRenderPassCreateInfo render_pass_create_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                sType
        nullptr,                                   // const void                    *pNext
        0,                                         // VkRenderPassCreateFlags        flags
        1,                                         // uint32_t                       attachmentCount
        attachment_descriptions,                   // const VkAttachmentDescription *pAttachments
        1,                                         // uint32_t                       subpassCount
        subpass_descriptions,                      // const VkSubpassDescription    *pSubpasses
        0,                                         // uint32_t                       dependencyCount
        nullptr                                    // const VkSubpassDependency     *pDependencies
    };

    Assert(vkCreateRenderPass(_device, &render_pass_create_info, nullptr, &_mainRenderPass) == VK_SUCCESS);
}

void SwapchainRendererBase::createFramebuffers()
{
    Assert(_swapChainImageCount > 0);

    _framebuffers.resize(_swapChainImageCount);
    _swapChainImageViews.resize(_swapChainImageCount);

    for (size_t i = 0; i < _swapChainImageCount; ++i)
    {
        VkImageViewCreateInfo image_view_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType                sType
            nullptr,                                  // const void                    *pNext
            0,                                        // VkImageViewCreateFlags         flags
            _swapChainImages[i],                      // VkImage                        image
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType                viewType
            _swapChainFormat,                         // VkFormat                       format
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

        Assert(vkCreateImageView(_device, &image_view_create_info, nullptr, &_swapChainImageViews[i]) == VK_SUCCESS);

        VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType                sType
            nullptr,                                   // const void                    *pNext
            0,                                         // VkFramebufferCreateFlags       flags
            _mainRenderPass,                           // VkRenderPass                   renderPass
            1,                                         // uint32_t                       attachmentCount
            &_swapChainImageViews[i],                  // const VkImageView             *pAttachments
            800,                                       // uint32_t                       width
            800,                                       // uint32_t                       height
            1                                          // uint32_t                       layers
        };

        Assert(vkCreateFramebuffer(_device, &framebuffer_create_info, nullptr, &_framebuffers[i]) == VK_SUCCESS);
    }
}

void SwapchainRendererBase::createCommandBufferPool()
{
    Assert(_swapChainImageCount > 0);

    VkCommandPoolCreateInfo cmd_pool_create_info = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // VkStructureType              sType
        nullptr,                                    // const void*                  pNext
        0,                                          // VkCommandPoolCreateFlags     flags
        _gfxQueueIndex                              // uint32_t                     queueFamilyIndex
    };

    Assert(vkCreateCommandPool(_device, &cmd_pool_create_info, nullptr, &_gfxCmdPool) == VK_SUCCESS);

    _gfxCmdBuffers.resize(_swapChainImageCount);

    VkCommandBufferAllocateInfo cmd_buffer_allocate_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType              sType
        nullptr,                                        // const void*                  pNext
        _gfxCmdPool,                                    // VkCommandPool                commandPool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel         level
        _swapChainImageCount                            // uint32_t                     bufferCount
    };

    Assert(vkAllocateCommandBuffers(_device, &cmd_buffer_allocate_info, &_gfxCmdBuffers[0]) == VK_SUCCESS);
}

PresentationInfo::PresentationInfo()
    : surface(VK_NULL_HANDLE)
    , surfaceCaps()
    , surfaceFormat({VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR})
    , swapchain(VK_NULL_HANDLE)
    , surfaceExtent({0, 0})
    , imageAvailableSemaphore(VK_NULL_HANDLE)
    , renderingFinishedSemaphore(VK_NULL_HANDLE)
    , imageCount(0)
    , images()
    , imageViews()
    , framebuffers()
    , renderPass(VK_NULL_HANDLE)
{
}

VulkanBackend::VulkanBackend()
    : vulkanLib(nullptr)
    , instance(VK_NULL_HANDLE)
    , physicalDevice(VK_NULL_HANDLE)
    , physicalDeviceInfo({0, 0, {}})
    , device(VK_NULL_HANDLE)
    , deviceInfo({VK_NULL_HANDLE, VK_NULL_HANDLE})
    , presentInfo()
    , debugCallback(VK_NULL_HANDLE)
{
}

void create_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE("Vulkan", MP_RED1);
    log_info(root, "vulkan: creating backend");

    log_debug(root, "vulkan: loading {}", REAPER_VK_LIB_NAME);
    backend.vulkanLib = dynlib::load(REAPER_VK_LIB_NAME);

    vulkan_load_exported_functions(backend.vulkanLib);
    vulkan_load_global_level_functions();

    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME, REAPER_VK_SWAPCHAIN_EXTENSION_NAME};

#if defined(REAPER_DEBUG)
    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

    log_info(root, "vulkan: using {} instance level extensions", extensions.size());
    for (auto& e : extensions)
        log_debug(root, "- {}", e);

    vulkan_instance_check_extensions(extensions);

    std::vector<const char*> layers;

#if defined(REAPER_DEBUG) && !defined(REAPER_PLATFORM_WINDOWS)
    layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

    log_info(root, "vulkan: using {} instance level layers", layers.size());
    for (auto& layer : layers)
        log_debug(root, "- {}", layer);

    vulkan_instance_check_layers(layers);

    VkApplicationInfo application_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType            sType
        nullptr,                            // const void                *pNext
        "MyGame",                           // const char                *pApplicationName
        VK_MAKE_VERSION(
            REAPER_VERSION_MAJOR, REAPER_VERSION_MINOR, REAPER_VERSION_PATCH), // uint32_t applicationVersion
        "Reaper",                                                              // const char                *pEngineName
        VK_MAKE_VERSION(REAPER_VERSION_MAJOR, REAPER_VERSION_MINOR, REAPER_VERSION_PATCH), // uint32_t engineVersion
        REAPER_VK_API_VERSION // uint32_t                   apiVersion
    };

    uint32_t layerCount = static_cast<uint32_t>(layers.size());
    uint32_t extensionCount = static_cast<uint32_t>(extensions.size());

    VkInstanceCreateInfo instance_create_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,          // VkStructureType            sType
        nullptr,                                         // const void*                pNext
        0,                                               // VkInstanceCreateFlags      flags
        &application_info,                               // const VkApplicationInfo   *pApplicationInfo
        layerCount,                                      // uint32_t                   enabledLayerCount
        (layerCount > 0 ? &layers[0] : nullptr),         // const char * const        *ppEnabledLayerNames
        extensionCount,                                  // uint32_t                   enabledExtensionCount
        (extensionCount > 0 ? &extensions[0] : nullptr), // const char * const        *ppEnabledExtensionNames
    };

    Assert(vkCreateInstance(&instance_create_info, nullptr, &backend.instance) == VK_SUCCESS,
           "cannot create Vulkan instance");

    vulkan_load_instance_level_functions(backend.instance);

#if defined(REAPER_DEBUG)
    log_debug(root, "vulkan: attaching debug callback");
    vulkan_setup_debug_callback(root, backend);
#endif

    WindowCreationDescriptor windowDescriptor;
    windowDescriptor.title = "Vulkan";
    windowDescriptor.width = 800;
    windowDescriptor.height = 600;
    windowDescriptor.fullscreen = false;

    log_info(root,
             "vulkan: creating window: size = {}x{}, title = '{}', fullscreen = {}",
             windowDescriptor.width,
             windowDescriptor.height,
             windowDescriptor.title,
             windowDescriptor.fullscreen);
    IWindow* window = createWindow(windowDescriptor);

    root.renderer->window = window;

    log_debug(root, "vulkan: creating presentation surface");
    vulkan_create_presentation_surface(backend.instance, backend.presentInfo.surface, window);

    log_debug(root, "vulkan: choosing physical device");
    vulkan_choose_physical_device(root, backend, backend.physicalDeviceInfo);

    log_debug(root, "vulkan: creating logical device");
    vulkan_create_logical_device(root, backend);

    SwapchainDescriptor swapchainDesc;
    swapchainDesc.preferredImageCount = 2; // Double buffering
    // swapchainDesc.preferredFormat = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
    swapchainDesc.preferredFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR};
    swapchainDesc.preferredExtent = {windowDescriptor.width, windowDescriptor.height};
    // swapchainDesc.preferredExtent = { 1920, 1080 };

    create_vulkan_swapchain(root, backend, swapchainDesc, backend.presentInfo);

    log_info(root, "vulkan: ready");
}

void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE("Vulkan", MP_RED1);
    log_info(root, "vulkan: destroying backend");

    destroy_vulkan_swapchain(root, backend, backend.presentInfo);

    log_debug(root, "vulkan: waiting for current work to finish");
    Assert(vkDeviceWaitIdle(backend.device) == VK_SUCCESS);

    log_debug(root, "vulkan: destroying logical device");
    vkDestroyDevice(backend.device, nullptr);

    log_debug(root, "vulkan: destroying presentation surface");
    vkDestroySurfaceKHR(backend.instance, backend.presentInfo.surface, nullptr);

    delete root.renderer->window;
    root.renderer->window = nullptr;

#if defined(REAPER_DEBUG)
    log_debug(root, "vulkan: detaching debug callback");
    vulkan_destroy_debug_callback(backend);
#endif

    vkDestroyInstance(backend.instance, nullptr);

    log_debug(root, "vulkan: unloading {}", REAPER_VK_LIB_NAME);
    Assert(backend.vulkanLib != nullptr);
    dynlib::close(backend.vulkanLib);
    backend.vulkanLib = nullptr;
}

void vulkan_instance_check_extensions(const std::vector<const char*>& extensions)
{
    uint32_t extensions_count = 0;
    Assert(vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, nullptr) == VK_SUCCESS);
    Assert(extensions_count > 0);

    std::vector<VkExtensionProperties> available_extensions(extensions_count);
    Assert(vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, &available_extensions[0]) == VK_SUCCESS);

    for (size_t i = 0; i < extensions.size(); ++i)
    {
        bool found = false;
        for (size_t j = 0; j < available_extensions.size(); ++j)
        {
            if (std::strcmp(available_extensions[j].extensionName, extensions[i]) == 0)
                found = true;
        }
        Assert(found, "extension not found");
    }
}

void vulkan_instance_check_layers(const std::vector<const char*>& layers)
{
    uint32_t layers_count = 0;
    Assert(vkEnumerateInstanceLayerProperties(&layers_count, nullptr) == VK_SUCCESS);
    Assert(layers_count > 0);

    std::vector<VkLayerProperties> available_layers(layers_count);
    Assert(vkEnumerateInstanceLayerProperties(&layers_count, &available_layers[0]) == VK_SUCCESS);

    for (size_t i = 0; i < layers.size(); ++i)
    {
        bool found = false;
        for (size_t j = 0; j < available_layers.size(); ++j)
        {
            if (std::strcmp(available_layers[j].layerName, layers[i]) == 0)
                found = true;
        }
        Assert(found, "layer not found");
    }
}

namespace
{
    VkBool32 debugReportCallback(VkDebugReportFlagsEXT /*flags*/,
                                 VkDebugReportObjectTypeEXT /*objectType*/,
                                 uint64_t /*object*/,
                                 size_t /*location*/,
                                 int32_t /*messageCode*/,
                                 const char* /*pLayerPrefix*/,
                                 const char* pMessage,
                                 void*       pUserData)
    {
        ReaperRoot* root = static_cast<ReaperRoot*>(pUserData);

        Assert(root != nullptr);
        // Assert(false, pMessage);

        log_warning(*root, "vulkan: {}", pMessage);
        return VK_FALSE;
    }
}

void vulkan_setup_debug_callback(ReaperRoot& root, VulkanBackend& backend)
{
    VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
    callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    callbackCreateInfo.pNext = nullptr;
    callbackCreateInfo.flags =
        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    callbackCreateInfo.pfnCallback = &debugReportCallback;
    callbackCreateInfo.pUserData = &root;

#if defined(REAPER_DEBUG)
    Assert(vkCreateDebugReportCallbackEXT(backend.instance, &callbackCreateInfo, nullptr, &backend.debugCallback)
           == VK_SUCCESS);
#endif
}

void vulkan_destroy_debug_callback(VulkanBackend& backend)
{
    Assert(backend.debugCallback != nullptr);

#if defined(REAPER_DEBUG)
    vkDestroyDebugReportCallbackEXT(backend.instance, backend.debugCallback, nullptr);
#endif

    backend.debugCallback = nullptr;
}

// Move this up
void vulkan_device_check_extensions(const std::vector<const char*>& extensions, VkPhysicalDevice physicalDevice)
{
    uint32_t extensions_count = 0;
    Assert(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensions_count, nullptr) == VK_SUCCESS);
    Assert(extensions_count > 0);

    std::vector<VkExtensionProperties> available_extensions(extensions_count);
    Assert(vkEnumerateDeviceExtensionProperties(
               physicalDevice, nullptr, &extensions_count, (extensions_count > 0 ? &available_extensions[0] : nullptr))
           == VK_SUCCESS);

    for (size_t i = 0; i < extensions.size(); ++i)
    {
        bool found = false;
        for (size_t j = 0; j < available_extensions.size(); ++j)
        {
            if (std::strcmp(available_extensions[j].extensionName, extensions[i]) == 0)
                found = true;
        }
        Assert(found, "extension not found");
    }
}

bool vulkan_check_physical_device(IWindow*                        window,
                                  VkPhysicalDevice                physical_device,
                                  VkSurfaceKHR                    presentationSurface,
                                  const std::vector<const char*>& extensions,
                                  uint32_t&                       queue_family_index,
                                  uint32_t&                       selected_present_queue_family_index)
{
    Assert(window != nullptr);

    vulkan_device_check_extensions(extensions, physical_device);

    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures   device_features;

    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    vkGetPhysicalDeviceFeatures(physical_device, &device_features);

    Assert(device_properties.apiVersion >= VK_MAKE_VERSION(1, 0, 0));
    Assert(device_properties.limits.maxImageDimension2D >= 4096);
    Assert(device_features.shaderClipDistance == VK_TRUE); // This is just checked, not enabled

    uint32_t queue_families_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

    Assert(queue_families_count > 0, "device doesn't have any queue families");
    if (queue_families_count == 0)
        return false;

    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_families_count);
    std::vector<VkBool32>                queue_present_support(queue_families_count);

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, &queue_family_properties[0]);

    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t present_queue_family_index = UINT32_MAX;

    for (uint32_t i = 0; i < queue_families_count; ++i)
    {
        Assert(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, presentationSurface, &queue_present_support[i])
               == VK_SUCCESS);

        if ((queue_family_properties[i].queueCount > 0)
            && (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            // Select first queue that supports graphics
            if (graphics_queue_family_index == UINT32_MAX)
                graphics_queue_family_index = i;

            Assert(vulkan_queue_family_has_presentation_support(physical_device, i, window)
                       == (queue_present_support[i] == VK_TRUE),
                   "Queue family presentation support mismatch.");

            // If there is queue that supports both graphics and present - prefer it
            if (queue_present_support[i])
            {
                queue_family_index = i;
                selected_present_queue_family_index = i;
                return true;
            }
        }
    }

    // We don't have queue that supports both graphics and present so we have to use separate queues
    for (uint32_t i = 0; i < queue_families_count; ++i)
    {
        if (queue_present_support[i] == VK_TRUE)
        {
            present_queue_family_index = i;
            break;
        }
    }

    Assert(graphics_queue_family_index != UINT32_MAX);
    Assert(present_queue_family_index != UINT32_MAX);

    queue_family_index = graphics_queue_family_index;
    selected_present_queue_family_index = present_queue_family_index;
    return true;
}

namespace
{
    const char* vulkan_physical_device_type_name(VkPhysicalDeviceType deviceType)
    {
        switch (deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "other";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "other";
        default:
            AssertUnreachable();
            break;
        }
        return "unknown";
    }
}

void vulkan_choose_physical_device(ReaperRoot& root, VulkanBackend& backend, PhysicalDeviceInfo& physicalDeviceInfo)
{
    uint32_t deviceCount = 0;

    Assert(vkEnumeratePhysicalDevices(backend.instance, &deviceCount, nullptr) == VK_SUCCESS);
    Assert(deviceCount > 0);

    log_debug(root, "vulkan: enumerating {} physical devices", deviceCount);

    std::vector<VkPhysicalDevice> availableDevices(deviceCount);
    Assert(vkEnumeratePhysicalDevices(backend.instance, &deviceCount, &availableDevices[0]) == VK_SUCCESS,
           "error occurred during physical devices enumeration");

    uint32_t selected_queue_family_index = UINT32_MAX;
    uint32_t selected_present_queue_family_index = UINT32_MAX;

    // Duplicated two times TODO merge
    std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkPhysicalDevice chosenPhysicalDevice = VK_NULL_HANDLE;
    for (auto& device : availableDevices)
    {
        if (vulkan_check_physical_device(root.renderer->window,
                                         device,
                                         backend.presentInfo.surface,
                                         extensions,
                                         selected_queue_family_index,
                                         selected_present_queue_family_index))
        {
            chosenPhysicalDevice = device;
            break;
        }
    }

    Assert(chosenPhysicalDevice != VK_NULL_HANDLE, "could not select physical device based on the chosen properties");

    physicalDeviceInfo.graphicsQueueIndex = selected_queue_family_index;
    physicalDeviceInfo.presentQueueIndex = selected_present_queue_family_index;

    vkGetPhysicalDeviceMemoryProperties(chosenPhysicalDevice, &physicalDeviceInfo.memory);

    // re-fetch device infos TODO avoid
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(chosenPhysicalDevice, &physicalDeviceProperties);

    log_info(root, "vulkan: selecting device '{}'", physicalDeviceProperties.deviceName);
    log_debug(root, "- type = {}", vulkan_physical_device_type_name(physicalDeviceProperties.deviceType));

    uint32_t apiVersion = physicalDeviceProperties.apiVersion;
    uint32_t driverVersion = physicalDeviceProperties.driverVersion;
    log_debug(root,
              "- api version = {}.{}.{}",
              VK_VERSION_MAJOR(apiVersion),
              VK_VERSION_MINOR(apiVersion),
              VK_VERSION_PATCH(apiVersion));
    log_debug(root,
              "- driver version = {}.{}.{}",
              VK_VERSION_MAJOR(driverVersion),
              VK_VERSION_MINOR(driverVersion),
              VK_VERSION_PATCH(driverVersion));

    log_debug(root,
              "- memory type count = {}, memory heap count = {}",
              physicalDeviceInfo.memory.memoryTypeCount,
              physicalDeviceInfo.memory.memoryHeapCount);

    for (u32 i = 0; i < physicalDeviceInfo.memory.memoryHeapCount; ++i)
    {
        VkMemoryHeap& heap = physicalDeviceInfo.memory.memoryHeaps[i];
        log_debug(root, "- heap {}: available size = {}, flags = {}", i, heap.size, heap.flags);
    }

    backend.physicalDevice = chosenPhysicalDevice;
}

void vulkan_create_logical_device(ReaperRoot& root, VulkanBackend& backend)
{
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::vector<float>                   queue_priorities = {1.0f};

    queue_create_infos.push_back({
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     // VkStructureType              sType
        nullptr,                                        // const void                  *pNext
        0,                                              // VkDeviceQueueCreateFlags     flags
        backend.physicalDeviceInfo.graphicsQueueIndex,  // uint32_t                     queueFamilyIndex
        static_cast<uint32_t>(queue_priorities.size()), // uint32_t                     queueCount
        &queue_priorities[0]                            // const float                 *pQueuePriorities
    });

    if (backend.physicalDeviceInfo.graphicsQueueIndex != backend.physicalDeviceInfo.presentQueueIndex)
    {
        queue_create_infos.push_back({
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     // VkStructureType              sType
            nullptr,                                        // const void                  *pNext
            0,                                              // VkDeviceQueueCreateFlags     flags
            backend.physicalDeviceInfo.presentQueueIndex,   // uint32_t                     queueFamilyIndex
            static_cast<uint32_t>(queue_priorities.size()), // uint32_t                     queueCount
            &queue_priorities[0]                            // const float                 *pQueuePriorities
        });
    }

    Assert(!queue_create_infos.empty());
    Assert(!queue_priorities.empty());
    Assert(queue_priorities.size() == queue_create_infos.size());

    std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    uint32_t queueCreateCount = static_cast<uint32_t>(queue_create_infos.size());
    uint32_t deviceExtensionCount = static_cast<uint32_t>(device_extensions.size());

    log_info(root, "vulkan: using {} device level extensions", device_extensions.size());
    for (auto& e : device_extensions)
        log_debug(root, "- {}", e);

    VkDeviceCreateInfo device_create_info = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType                    sType
        nullptr,                              // const void                        *pNext
        0,                                    // VkDeviceCreateFlags                flags
        queueCreateCount,                     // uint32_t                           queueCreateInfoCount
        &queue_create_infos[0],               // const VkDeviceQueueCreateInfo     *pQueueCreateInfos
        0,                                    // uint32_t                           enabledLayerCount
        nullptr,                              // const char * const                *ppEnabledLayerNames
        deviceExtensionCount,                 // uint32_t                           enabledExtensionCount
        (deviceExtensionCount > 0 ? &device_extensions[0] : nullptr), // const char * const *ppEnabledExtensionNames
        nullptr // const VkPhysicalDeviceFeatures    *pEnabledFeatures
    };

    Assert(vkCreateDevice(backend.physicalDevice, &device_create_info, nullptr, &backend.device) == VK_SUCCESS,
           "could not create Vulkan device");

    vulkan_load_device_level_functions(backend.device);

    vkGetDeviceQueue(
        backend.device, backend.physicalDeviceInfo.graphicsQueueIndex, 0, &backend.deviceInfo.graphicsQueue);
    vkGetDeviceQueue(backend.device, backend.physicalDeviceInfo.presentQueueIndex, 0, &backend.deviceInfo.presentQueue);
}
