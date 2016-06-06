////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "SwapchainRendererBase.h"

#include <cstring>

#include "PresentationSurface.h"

using namespace vk;

namespace
{
    bool checkExtensionAvailability(const char *extension_name, const std::vector<VkExtensionProperties> &available_extensions)
    {
        for (size_t i = 0; i < available_extensions.size(); ++i)
        {
            if (std::strcmp(available_extensions[i].extensionName, extension_name) == 0)
                return true;
        }
        return false;
    }

    bool checkLayerAvailability(const char *layer_name, const std::vector<VkLayerProperties> &available_layers)
    {
        for (size_t i = 0; i < available_layers.size(); ++i)
        {
            if (std::strcmp(available_layers[i].layerName, layer_name) == 0)
                return true;
        }
        return false;
    }

    bool checkPhysicalDeviceProperties(VkPhysicalDevice physical_device, VkSurfaceKHR presentationSurface, uint32_t& queue_family_index, uint32_t& selected_present_queue_family_index)
    {
        uint32_t extensions_count = 0;
        Assert(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensions_count, nullptr) == VK_SUCCESS);
        Assert(extensions_count > 0);

        std::vector<VkExtensionProperties> available_extensions(extensions_count);
        Assert(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensions_count, &available_extensions[0]) == VK_SUCCESS);

        std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        Assert(extensions.size() == 1);

        for(size_t i = 0; i < extensions.size(); ++i)
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

        uint32_t queue_families_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

        Assert(queue_families_count > 0, "device doesn't have any queue families");
        if (queue_families_count == 0)
            return false;

        std::vector<VkQueueFamilyProperties> queue_family_properties( queue_families_count );
        std::vector<VkBool32>                queue_present_support( queue_families_count );

        vkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_families_count, &queue_family_properties[0] );

        uint32_t graphics_queue_family_index = UINT32_MAX;
        uint32_t present_queue_family_index = UINT32_MAX;

        for (uint32_t i = 0; i < queue_families_count; ++i)
        {
            Assert(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, presentationSurface, &queue_present_support[i]) == VK_SUCCESS);

            if ((queue_family_properties[i].queueCount > 0) &&
                (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) )
            {
                // Select first queue that supports graphics
                if( graphics_queue_family_index == UINT32_MAX ) {
                    graphics_queue_family_index = i;
                }

                // If there is queue that supports both graphics and present - prefer it
                if( queue_present_support[i] ) {
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

    VkBool32 debugReportCallback(
        VkDebugReportFlagsEXT       /*flags*/,
        VkDebugReportObjectTypeEXT  /*objectType*/,
        uint64_t                    /*object*/,
        size_t                      /*location*/,
        int32_t                     /*messageCode*/,
        const char*                 /*pLayerPrefix*/,
        const char*                 pMessage,
        void*                       /*pUserData*/)
    {
        //         Assert(false, pMessage);
        std::cerr << pMessage << std::endl;
        return VK_FALSE;
    }
}

void SwapchainRendererBase::startup(Window* window)
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

    #define REAPER_VK_EXPORTED_FUNCTION(func) {                                 \
        func = (PFN_##func)dynlib::getSymbol(_vulkanLib, #func); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    #define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func) {                             \
        func = (PFN_##func)vkGetInstanceProcAddr(nullptr, #func);               \
        Assert(func != nullptr, "could not load global level vk function"); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    uint32_t extensions_count = 0;
    Assert(vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, nullptr) == VK_SUCCESS);
    Assert(extensions_count > 0);

    std::vector<VkExtensionProperties> available_extensions(extensions_count);
    Assert(vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, &available_extensions[0]) == VK_SUCCESS);

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        REAPER_VK_SWAPCHAIN_EXTENSION_NAME
    };

    Assert(extensions.size() == 2);

    #if defined(REAPER_DEBUG)
    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    #endif

    for(size_t i = 0; i < extensions.size(); ++i)
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

    std::vector<const char*>  enabledInstanceLayers;

    uint32_t layers_count = 0;
    Assert(vkEnumerateInstanceLayerProperties(&layers_count, nullptr) == VK_SUCCESS);
    Assert(layers_count > 0);

    std::vector<VkLayerProperties> available_layers(layers_count);
    Assert(vkEnumerateInstanceLayerProperties(&layers_count, &available_layers[0]) == VK_SUCCESS);

    #if defined(REAPER_DEBUG)
    enabledInstanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
    #endif

    for(size_t i = 0; i < enabledInstanceLayers.size(); ++i)
        Assert(checkLayerAvailability(enabledInstanceLayers[i], available_layers));

    VkInstanceCreateInfo instance_create_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,     // VkStructureType            sType
        nullptr,                                    // const void*                pNext
        0,                                          // VkInstanceCreateFlags      flags
        &application_info,                          // const VkApplicationInfo   *pApplicationInfo
        static_cast<uint32_t>(enabledInstanceLayers.size()),  // uint32_t                   enabledLayerCount
        &enabledInstanceLayers[0],                  // const char * const        *ppEnabledLayerNames
        static_cast<uint32_t>(extensions.size()),   // uint32_t                   enabledExtensionCount
        &extensions[0]                              // const char * const        *ppEnabledExtensionNames
    };

    Assert(vkCreateInstance(&instance_create_info, nullptr, &_instance) == VK_SUCCESS, "cannot create Vulkan instance");

    #define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func) {                           \
        func = (PFN_##func)vkGetInstanceProcAddr(_instance, #func);             \
        Assert(func != nullptr, "could not load instance level vk function"); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    #if defined(REAPER_DEBUG)
    /* Setup callback creation information */
    VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
    callbackCreateInfo.sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    callbackCreateInfo.pNext       = nullptr;
    callbackCreateInfo.flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    callbackCreateInfo.pfnCallback = &debugReportCallback;
    callbackCreateInfo.pUserData   = nullptr;

    Assert(vkCreateDebugReportCallbackEXT(_instance, &callbackCreateInfo, nullptr, &_debugCallback) == VK_SUCCESS);
    #endif

    createPresentationSurface(_instance, window->GetParameters(), _presentationSurface);

    uint32_t deviceNo = 0;

    Assert(vkEnumeratePhysicalDevices(_instance, &deviceNo, nullptr) == VK_SUCCESS);
    Assert(deviceNo > 0);

    std::vector<VkPhysicalDevice> physical_devices(deviceNo);
    Assert(vkEnumeratePhysicalDevices(_instance, &deviceNo, &physical_devices[0]) == VK_SUCCESS, "error occurred during physical devices enumeration");

    uint32_t selected_queue_family_index = UINT32_MAX;
    uint32_t selected_present_queue_family_index = UINT32_MAX;

    for (uint32_t i = 0; i < deviceNo; ++i)
    {
        if (checkPhysicalDeviceProperties(physical_devices[i], _presentationSurface, selected_queue_family_index, selected_present_queue_family_index))
        {
            _physicalDevice = physical_devices[i];
            break;
        }
    }
    Assert(_physicalDevice != VK_NULL_HANDLE, "could not select physical device based on the chosen properties");

    _prsQueueIndex = selected_present_queue_family_index;
    _gfxQueueIndex = selected_queue_family_index;

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::vector<float> queue_priorities = { 1.0f };

    queue_create_infos.push_back( {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,       // VkStructureType              sType
        nullptr,                                          // const void                  *pNext
        0,                                                // VkDeviceQueueCreateFlags     flags
        selected_queue_family_index,                    // uint32_t                     queueFamilyIndex
        static_cast<uint32_t>(queue_priorities.size()), // uint32_t                     queueCount
                                  &queue_priorities[0]                              // const float                 *pQueuePriorities
    } );

    if( selected_queue_family_index != selected_present_queue_family_index ) {
        queue_create_infos.push_back( {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     // VkStructureType              sType
            nullptr,                                        // const void                  *pNext
            0,                                              // VkDeviceQueueCreateFlags     flags
            selected_present_queue_family_index,            // uint32_t                     queueFamilyIndex
            static_cast<uint32_t>(queue_priorities.size()), // uint32_t                     queueCount
                                      &queue_priorities[0]                            // const float                 *pQueuePriorities
        } );
    }

    std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    std::vector<const char*> device_layers;

    #if defined(REAPER_DEBUG)
    device_layers.push_back("VK_LAYER_LUNARG_standard_validation");
    #endif

    VkDeviceCreateInfo device_create_info = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,           // VkStructureType                    sType
        nullptr,                                        // const void                        *pNext
        0,                                              // VkDeviceCreateFlags                flags
        static_cast<uint32_t>(queue_create_infos.size()),// uint32_t                           queueCreateInfoCount
        &queue_create_infos[0],                         // const VkDeviceQueueCreateInfo     *pQueueCreateInfos
        static_cast<uint32_t>(device_layers.size()),    // uint32_t                           enabledLayerCount
        &device_layers[0],                              // const char * const                *ppEnabledLayerNames
        static_cast<uint32_t>(device_extensions.size()),// uint32_t                           enabledExtensionCount
        &device_extensions[0],                          // const char * const                *ppEnabledExtensionNames
        nullptr                                         // const VkPhysicalDeviceFeatures    *pEnabledFeatures
    };

    Assert(vkCreateDevice(_physicalDevice, &device_create_info, nullptr, &_device) == VK_SUCCESS, "could not create Vulkan device");

    #define REAPER_VK_DEVICE_LEVEL_FUNCTION(func) {                             \
        func = (PFN_##func)vkGetDeviceProcAddr(_device, #func);                 \
        Assert(func != nullptr, "could not load device level vk function"); }
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
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,      // VkStructureType          sType
        nullptr,                                      // const void*              pNext
        0                                             // VkSemaphoreCreateFlags   flags
    };

    Assert(vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_imageAvailableSemaphore) == VK_SUCCESS);
    Assert(vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_renderingFinishedSemaphore) == VK_SUCCESS);
}

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
            VkExtent2D swap_chain_extent = { 300, 300 };
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

void SwapchainRendererBase::createSwapChain()
{
    VkSurfaceCapabilitiesKHR surface_capabilities;
    Assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _presentationSurface, &surface_capabilities) == VK_SUCCESS);

    uint32_t formats_count;
    Assert(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _presentationSurface, &formats_count, nullptr) == VK_SUCCESS);
    Assert(formats_count > 0);

    std::vector<VkSurfaceFormatKHR> surface_formats(formats_count);
    Assert(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _presentationSurface, &formats_count, &surface_formats[0]) == VK_SUCCESS);

    uint32_t present_modes_count;
    Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _presentationSurface, &present_modes_count, nullptr ) == VK_SUCCESS);
    Assert(present_modes_count > 0);

    std::vector<VkPresentModeKHR> present_modes(present_modes_count);
    Assert(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _presentationSurface, &present_modes_count, &present_modes[0]) == VK_SUCCESS);

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
        _presentationSurface,                         // VkSurfaceKHR                   surface
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

    Assert(vkCreateSwapchainKHR(_device, &swap_chain_create_info, nullptr, &_swapChain) == VK_SUCCESS);
    Assert(vkGetSwapchainImagesKHR(_device, _swapChain, &_swapChainImageCount, nullptr) == VK_SUCCESS);
    Assert(_swapChainImageCount > 0);

    _swapChainFormat = desired_format.format;
    _swapChainImages.resize(_swapChainImageCount);
    Assert(vkGetSwapchainImagesKHR(_device, _swapChain, &_swapChainImageCount, &_swapChainImages[0]) == VK_SUCCESS);
}

void SwapchainRendererBase::createRenderPass()
{
    VkAttachmentDescription attachment_descriptions[] = {
        {
            0,                                          // VkAttachmentDescriptionFlags   flags
            _swapChainFormat,                           // VkFormat                       format
            VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits          samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,                // VkAttachmentLoadOp             loadOp
            VK_ATTACHMENT_STORE_OP_STORE,               // VkAttachmentStoreOp            storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // VkAttachmentLoadOp             stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,           // VkAttachmentStoreOp            stencilStoreOp
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,            // VkImageLayout                  initialLayout;
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR             // VkImageLayout                  finalLayout
        }
    };

    VkAttachmentReference color_attachment_references[] = {
        {
            0,                                          // uint32_t                       attachment
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL    // VkImageLayout                  layout
        }
    };

    VkSubpassDescription subpass_descriptions[] = {
        {
            0,                                          // VkSubpassDescriptionFlags      flags
            VK_PIPELINE_BIND_POINT_GRAPHICS,            // VkPipelineBindPoint            pipelineBindPoint
            0,                                          // uint32_t                       inputAttachmentCount
            nullptr,                                    // const VkAttachmentReference   *pInputAttachments
            1,                                          // uint32_t                       colorAttachmentCount
            color_attachment_references,                // const VkAttachmentReference   *pColorAttachments
            nullptr,                                    // const VkAttachmentReference   *pResolveAttachments
            nullptr,                                    // const VkAttachmentReference   *pDepthStencilAttachment
            0,                                          // uint32_t                       preserveAttachmentCount
            nullptr                                     // const uint32_t*                pPreserveAttachments
        }
    };

    VkRenderPassCreateInfo render_pass_create_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,    // VkStructureType                sType
        nullptr,                                      // const void                    *pNext
        0,                                            // VkRenderPassCreateFlags        flags
        1,                                            // uint32_t                       attachmentCount
        attachment_descriptions,                      // const VkAttachmentDescription *pAttachments
        1,                                            // uint32_t                       subpassCount
        subpass_descriptions,                         // const VkSubpassDescription    *pSubpasses
        0,                                            // uint32_t                       dependencyCount
        nullptr                                       // const VkSubpassDependency     *pDependencies
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
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType                sType
            nullptr,                                    // const void                    *pNext
            0,                                          // VkImageViewCreateFlags         flags
            _swapChainImages[i],                        // VkImage                        image
            VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType                viewType
            _swapChainFormat,                           // VkFormat                       format
            {                                           // VkComponentMapping             components
                VK_COMPONENT_SWIZZLE_IDENTITY,              // VkComponentSwizzle             r
                VK_COMPONENT_SWIZZLE_IDENTITY,              // VkComponentSwizzle             g
                VK_COMPONENT_SWIZZLE_IDENTITY,              // VkComponentSwizzle             b
                VK_COMPONENT_SWIZZLE_IDENTITY               // VkComponentSwizzle             a
            },
            {                                           // VkImageSubresourceRange        subresourceRange
                VK_IMAGE_ASPECT_COLOR_BIT,                  // VkImageAspectFlags             aspectMask
                0,                                          // uint32_t                       baseMipLevel
                1,                                          // uint32_t                       levelCount
                0,                                          // uint32_t                       baseArrayLayer
                1                                           // uint32_t                       layerCount
            }
        };

        Assert(vkCreateImageView(_device, &image_view_create_info, nullptr, &_swapChainImageViews[i]) == VK_SUCCESS);

        VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,  // VkStructureType                sType
            nullptr,                                    // const void                    *pNext
            0,                                          // VkFramebufferCreateFlags       flags
            _mainRenderPass,                                // VkRenderPass                   renderPass
            1,                                          // uint32_t                       attachmentCount
            &_swapChainImageViews[i],                            // const VkImageView             *pAttachments
            300,                                        // uint32_t                       width
            300,                                        // uint32_t                       height
            1                                           // uint32_t                       layers
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
