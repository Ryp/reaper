////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VulkanRenderer.h"

#include <cstring>

#include "core/fs/FileLoading.h"

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
        std::cerr << pMessage << std::endl;
        return VK_FALSE;
    }

    struct VSPushConstants {
        float scale;
    };
}

void VulkanRenderer::startup(Window* window)
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

    _pipeline = VK_NULL_HANDLE;
    _pipelineLayout = VK_NULL_HANDLE;

    _debugCallback = VK_NULL_HANDLE;

    _gfxCmdPool = VK_NULL_HANDLE;
    _gfxCmdBuffers.clear();
    _gfxQueueIndex = UINT32_MAX;

    _deviceMemory = VK_NULL_HANDLE;
    _vertexBuffer = VK_NULL_HANDLE;
    _indexBuffer = VK_NULL_HANDLE;

    _renderPass = VK_NULL_HANDLE;

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
    //_presentQueueIndex = selected_present_queue_family_index;
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
    createPipeline();
    createMeshBuffers();
    createCommandBuffers();
}

void VulkanRenderer::shutdown()
{
    Assert(vkDeviceWaitIdle(_device) == VK_SUCCESS);

    //vkFreeCommandBuffers(_device, _presentCmdPool, static_cast<uint32_t>(_presentCmdBuffers.size()), &_presentCmdBuffers[0]);
    //vkDestroyCommandPool(_device, _presentCmdPool, nullptr);

    vkFreeCommandBuffers(_device, _gfxCmdPool, static_cast<uint32_t>(_gfxCmdBuffers.size()), &_gfxCmdBuffers[0]);
    vkDestroyCommandPool(_device, _gfxCmdPool, nullptr);

    vkDestroyPipeline(_device, _pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);

    vkDestroyRenderPass(_device, _renderPass, nullptr);

    Assert(vkDeviceWaitIdle(_device) == VK_SUCCESS);
    vkDestroySemaphore(_device, _imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(_device, _renderingFinishedSemaphore, nullptr);
    vkDestroySwapchainKHR(_device, _swapChain, nullptr);
    vkDestroyDevice(_device, nullptr);

#if defined(REAPER_DEBUG)
    vkDestroyDebugReportCallbackEXT(_instance, _debugCallback, nullptr);
#endif

    vkDestroyInstance(_instance, nullptr);

    dynlib::close(_vulkanLib);
}

void VulkanRenderer::render()
{
    uint32_t image_index;

    VkResult result = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphore, VK_NULL_HANDLE, &image_index);
    switch(result)
    {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR:
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            Assert("Resize not implemented");
            break;
        default:
            Assert("Problem occurred during swap chain image acquisition");
            break;
    }

    VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,      // VkStructureType              sType
        nullptr,                            // const void                  *pNext
        1,                                  // uint32_t                     waitSemaphoreCount
        &_imageAvailableSemaphore,          // const VkSemaphore           *pWaitSemaphores
        &wait_dst_stage_mask,               // const VkPipelineStageFlags  *pWaitDstStageMask;
        1,                                  // uint32_t                     commandBufferCount
        &_gfxCmdBuffers[image_index],   // const VkCommandBuffer       *pCommandBuffers
        1,                                  // uint32_t                     signalSemaphoreCount
        &_renderingFinishedSemaphore        // const VkSemaphore           *pSignalSemaphores
    };

    Assert(vkQueueSubmit(_graphicsQueue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS);

    VkPresentInfoKHR present_info = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // VkStructureType              sType
        nullptr,                            // const void                  *pNext
        1,                                  // uint32_t                     waitSemaphoreCount
        &_renderingFinishedSemaphore,       // const VkSemaphore           *pWaitSemaphores
        1,                                  // uint32_t                     swapchainCount
        &_swapChain,                        // const VkSwapchainKHR        *pSwapchains
        &image_index,                       // const uint32_t              *pImageIndices
        nullptr                             // VkResult                    *pResults
    };
    result = vkQueuePresentKHR(_presentationQueue, &present_info);

    switch( result ) {
        case VK_SUCCESS:
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            Assert(false, "not supported");
            break;
        default:
            Assert(false, "problem occurred during image presentation");
            break;
    }
    /*uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphore, VK_NULL_HANDLE, &image_index);
    switch(result)
    {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR:
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            Assert("Resize not implemented");
            break;
        default:
            Assert("Problem occurred during swap chain image acquisition");
            break;
    }

    VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,      // VkStructureType              sType
        nullptr,                            // const void                  *pNext
        1,                                  // uint32_t                     waitSemaphoreCount
        &_imageAvailableSemaphore,          // const VkSemaphore           *pWaitSemaphores
        &wait_dst_stage_mask,               // const VkPipelineStageFlags  *pWaitDstStageMask;
        1,                                  // uint32_t                     commandBufferCount
        &_gfxCmdBuffers[image_index],   // const VkCommandBuffer       *pCommandBuffers
        1,                                  // uint32_t                     signalSemaphoreCount
        &_renderingFinishedSemaphore        // const VkSemaphore           *pSignalSemaphores
    };

    Assert(vkQueueSubmit(_presentationQueue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS);

    VkPresentInfoKHR present_info = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // VkStructureType              sType
        nullptr,                            // const void                  *pNext
        1,                                  // uint32_t                     waitSemaphoreCount
        &_renderingFinishedSemaphore,       // const VkSemaphore           *pWaitSemaphores
        1,                                  // uint32_t                     swapchainCount
        &_swapChain,                        // const VkSwapchainKHR        *pSwapchains
        &image_index,                       // const uint32_t              *pImageIndices
        nullptr                             // VkResult                    *pResults
    };
    result = vkQueuePresentKHR(_presentationQueue, &present_info);

    switch( result ) {
        case VK_SUCCESS:
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            Assert(false, "not supported");
            break;
        default:
            Assert(false, "problem occurred during image presentation");
        break;
    }*/
}

void VulkanRenderer::createSemaphores()
{
    VkSemaphoreCreateInfo semaphore_create_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,      // VkStructureType          sType
        nullptr,                                      // const void*              pNext
        0                                             // VkSemaphoreCreateFlags   flags
    };

    Assert(vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_imageAvailableSemaphore) == VK_SUCCESS);
    Assert(vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_renderingFinishedSemaphore) == VK_SUCCESS);
}

static VkSurfaceFormatKHR GetSwapChainFormat(std::vector<VkSurfaceFormatKHR> &surface_formats)
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

static VkExtent2D GetSwapChainExtent(VkSurfaceCapabilitiesKHR& surface_capabilities)
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

static VkImageUsageFlags GetSwapChainUsageFlags(VkSurfaceCapabilitiesKHR& surface_capabilities)
{
    // Color attachment flag must always be supported
    // We can define other usage flags but we always need to check if they are supported
    if( surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) {
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    Assert("VK_IMAGE_USAGE_TRANSFER_DST image usage is not supported by the swap chain!");
    //     << "Supported swap chain's image usages include:" << std::endl
    //     << (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT              ? "    VK_IMAGE_USAGE_TRANSFER_SRC\n" : "")
    //     << (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT              ? "    VK_IMAGE_USAGE_TRANSFER_DST\n" : "")
    //     << (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT                   ? "    VK_IMAGE_USAGE_SAMPLED\n" : "")
    //     << (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT                   ? "    VK_IMAGE_USAGE_STORAGE\n" : "")
    //     << (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT          ? "    VK_IMAGE_USAGE_COLOR_ATTACHMENT\n" : "")
    //     << (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT  ? "    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT\n" : "")
    //     << (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT      ? "    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT\n" : "")
    //     << (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT          ? "    VK_IMAGE_USAGE_INPUT_ATTACHMENT" : "")
    //     << std::endl;
    return static_cast<VkImageUsageFlags>(-1);
}

static VkSurfaceTransformFlagBitsKHR GetSwapChainTransform(VkSurfaceCapabilitiesKHR& surface_capabilities)
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

static VkPresentModeKHR GetSwapChainPresentMode(std::vector<VkPresentModeKHR>& present_modes)
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
    Assert("FIFO present mode is not supported by the swap chain!");
    return static_cast<VkPresentModeKHR>(-1);
}

void VulkanRenderer::createSwapChain()
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

    uint32_t swapchain_image_count = 0;
    Assert(vkGetSwapchainImagesKHR(_device, _swapChain, &swapchain_image_count, nullptr) == VK_SUCCESS);
    Assert(swapchain_image_count > 0);

    _swapChainFormat = desired_format.format;
    _swapChainImages.resize(swapchain_image_count);
    Assert(vkGetSwapchainImagesKHR(_device, _swapChain, &swapchain_image_count, &_swapChainImages[0]) == VK_SUCCESS);
}

void VulkanRenderer::createCommandBuffers()
{
    VkCommandPoolCreateInfo cmd_pool_create_info = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,     // VkStructureType              sType
        nullptr,                                        // const void*                  pNext
        0,                                              // VkCommandPoolCreateFlags     flags
        _gfxQueueIndex                              // uint32_t                     queueFamilyIndex
    };

    uint32_t image_count = static_cast<uint32_t>(_swapChainImages.size());

    Assert(vkCreateCommandPool(_device, &cmd_pool_create_info, nullptr, &_gfxCmdPool) == VK_SUCCESS);

    _gfxCmdBuffers.resize(image_count);

    VkCommandBufferAllocateInfo cmd_buffer_allocate_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType              sType
        nullptr,                                        // const void*                  pNext
        _gfxCmdPool,                                // VkCommandPool                commandPool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel         level
        image_count                                     // uint32_t                     bufferCount
    };

    Assert(vkAllocateCommandBuffers(_device, &cmd_buffer_allocate_info, &_gfxCmdBuffers[0]) == VK_SUCCESS);

    // sauce

    VkCommandBufferBeginInfo graphics_commandd_buffer_begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,    // VkStructureType                        sType
        nullptr,                                        // const void                            *pNext
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,   // VkCommandBufferUsageFlags              flags
        nullptr                                         // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
    };

    VkImageSubresourceRange image_subresource_range = {
        VK_IMAGE_ASPECT_COLOR_BIT,                      // VkImageAspectFlags             aspectMask
        0,                                              // uint32_t                       baseMipLevel
        1,                                              // uint32_t                       levelCount
        0,                                              // uint32_t                       baseArrayLayer
        1                                               // uint32_t                       layerCount
    };

    VkClearValue clear_value = {
        { 1.0f, 0.8f, 0.4f, 0.0f },                     // VkClearColorValue              color
    };

    for (size_t i = 0; i < image_count; ++i)
    {
        Assert(vkBeginCommandBuffer(_gfxCmdBuffers[i], &graphics_commandd_buffer_begin_info) == VK_SUCCESS);

        if (_presentationQueue != _graphicsQueue)
        {
            VkImageMemoryBarrier barrier_from_present_to_draw = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType                sType
                nullptr,                                    // const void                    *pNext
                VK_ACCESS_MEMORY_READ_BIT,                  // VkAccessFlags                  srcAccessMask
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags                  dstAccessMask
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,            // VkImageLayout                  oldLayout
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,            // VkImageLayout                  newLayout
                _prsQueueIndex,              // uint32_t                       srcQueueFamilyIndex
                _gfxQueueIndex,             // uint32_t                       dstQueueFamilyIndex
                _swapChainImages[i],                // VkImage                        image
                image_subresource_range                     // VkImageSubresourceRange        subresourceRange
            };
            vkCmdPipelineBarrier(_gfxCmdBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_present_to_draw );
        }

        VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,     // VkStructureType                sType
            nullptr,                                      // const void                    *pNext
            _renderPass,                            // VkRenderPass                   renderPass
            _framebuffers[i],                       // VkFramebuffer                  framebuffer
            {                                             // VkRect2D                       renderArea
                {                                           // VkOffset2D                     offset
                    0,                                          // int32_t                        x
                    0                                           // int32_t                        y
                },
                {                                           // VkExtent2D                     extent
                    300,                                        // int32_t                        width
                    300,                                        // int32_t                        height
                }
            },
            1,                                            // uint32_t                       clearValueCount
            &clear_value                                  // const VkClearValue            *pClearValues
        };

        vkCmdBeginRenderPass(_gfxCmdBuffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );

        VSPushConstants constants;
        constants.scale = 0.5f;

        vkCmdPushConstants(_gfxCmdBuffers[i], _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);

        vkCmdBindPipeline(_gfxCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

        VkDeviceSize offsets [] = { 0 };
        vkCmdBindIndexBuffer (_gfxCmdBuffers[i], _indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers (_gfxCmdBuffers[i], 0, 1, &_vertexBuffer, offsets);
        vkCmdDrawIndexed (_gfxCmdBuffers[i], 6, 1, 0, 0, 0);

        //vkCmdDraw(_gfxCmdBuffers[i], 3, 1, 0, 0 );

        vkCmdEndRenderPass(_gfxCmdBuffers[i] );

        if (_presentationQueue != _graphicsQueue) {
            VkImageMemoryBarrier barrier_from_draw_to_present = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,       // VkStructureType              sType
                nullptr,                                      // const void                  *pNext
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,         // VkAccessFlags                srcAccessMask
                VK_ACCESS_MEMORY_READ_BIT,                    // VkAccessFlags                dstAccessMask
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,              // VkImageLayout                oldLayout
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,              // VkImageLayout                newLayout
                _gfxQueueIndex,               // uint32_t                     srcQueueFamilyIndex
                _prsQueueIndex,                // uint32_t                     dstQueueFamilyIndex
                _swapChainImages[i],                  // VkImage                      image
                image_subresource_range                       // VkImageSubresourceRange      subresourceRange
            };
            vkCmdPipelineBarrier(_gfxCmdBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_draw_to_present );
        }
        Assert(vkEndCommandBuffer(_gfxCmdBuffers[i]) == VK_SUCCESS);
    }
    /*
    VkClearColorValue clear_color = {
        { 1.0f, 0.8f, 0.4f, 0.0f }
    };

    VkImageSubresourceRange image_subresource_range = {
        VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags                     aspectMask
        0,                                            // uint32_t                               baseMipLevel
        1,                                            // uint32_t                               levelCount
        0,                                            // uint32_t                               baseArrayLayer
        1                                             // uint32_t                               layerCount
    };

    for (uint32_t i = 0; i < image_count; ++i)
    {
        VkImageMemoryBarrier barrier_from_present_to_clear = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType                        sType
            nullptr,                                    // const void                            *pNext
            VK_ACCESS_MEMORY_READ_BIT,                  // VkAccessFlags                          srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,               // VkAccessFlags                          dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout                          oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,       // VkImageLayout                          newLayout
            _presentQueueIndex,                         // uint32_t                               srcQueueFamilyIndex
            _presentQueueIndex,                         // uint32_t                               dstQueueFamilyIndex
            _swapChainImages[i],                       // VkImage                                image
            image_subresource_range                     // VkImageSubresourceRange                subresourceRange
        };

        VkImageMemoryBarrier barrier_from_clear_to_present = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType                        sType
            nullptr,                                    // const void                            *pNext
            VK_ACCESS_TRANSFER_WRITE_BIT,               // VkAccessFlags                          srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                  // VkAccessFlags                          dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,       // VkImageLayout                          oldLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,            // VkImageLayout                          newLayout
            _presentQueueIndex,             // uint32_t                               srcQueueFamilyIndex
            _presentQueueIndex,             // uint32_t                               dstQueueFamilyIndex
            _swapChainImages[i],                       // VkImage                                image
            image_subresource_range                     // VkImageSubresourceRange                subresourceRange
        };

        Assert(vkBeginCommandBuffer(_presentCmdBuffers[i], &cmd_buffer_begin_info) == VK_SUCCESS);
        vkCmdPipelineBarrier(_presentCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_present_to_clear);

        vkCmdClearColorImage(_presentCmdBuffers[i], _swapChainImages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_subresource_range );

        vkCmdPipelineBarrier(_presentCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_clear_to_present);
        Assert(vkEndCommandBuffer(_presentCmdBuffers[i]) == VK_SUCCESS);
    }*/
}

void VulkanRenderer::createRenderPass()
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

    Assert(vkCreateRenderPass(_device, &render_pass_create_info, nullptr, &_renderPass) == VK_SUCCESS);
}

void VulkanRenderer::createFramebuffers()
{
    const size_t swapchain_image_count = _swapChainImages.size();

    _framebuffers.resize(swapchain_image_count);
    _swapChainImageViews.resize(swapchain_image_count);

    for (size_t i = 0; i < swapchain_image_count; ++i)
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
            _renderPass,                                // VkRenderPass                   renderPass
            1,                                          // uint32_t                       attachmentCount
            &_swapChainImageViews[i],                            // const VkImageView             *pAttachments
            300,                                        // uint32_t                       width
            300,                                        // uint32_t                       height
            1                                           // uint32_t                       layers
        };

        Assert(vkCreateFramebuffer(_device, &framebuffer_create_info, nullptr, &_framebuffers[i]) == VK_SUCCESS);
    }
}

static void createShaderModule(VkShaderModule& shaderModule, VkDevice device, const std::string& fileName)
{
    std::vector<char> fileContents = readWholeFile(fileName);

    VkShaderModuleCreateInfo shader_module_create_info =
    {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        nullptr,
        0,
        fileContents.size(),
        reinterpret_cast<const uint32_t*>(&fileContents[0])
    };

    Assert(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shaderModule) == VK_SUCCESS);
}

void VulkanRenderer::createPipeline()
{
    VkShaderModule vertModule;
    VkShaderModule fragModule;

    createShaderModule(vertModule, _device, "./build/spv/transform.vert.spv");
    createShaderModule(fragModule, _device, "./build/spv/transform.frag.spv");

    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos = {
        // Vertex shader
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,        // VkStructureType                                sType
            nullptr,                                                    // const void                                    *pNext
            0,                                                          // VkPipelineShaderStageCreateFlags               flags
            VK_SHADER_STAGE_VERTEX_BIT,                                 // VkShaderStageFlagBits                          stage
            vertModule,                                                 // VkShaderModule                                 module
            "main",                                                     // const char                                    *pName
            nullptr                                                     // const VkSpecializationInfo                    *pSpecializationInfo
        },
        // Fragment shader
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,        // VkStructureType                                sType
            nullptr,                                                    // const void                                    *pNext
            0,                                                          // VkPipelineShaderStageCreateFlags               flags
            VK_SHADER_STAGE_FRAGMENT_BIT,                               // VkShaderStageFlagBits                          stage
            fragModule,                                                 // VkShaderModule                                 module
            "main",                                                     // const char                                    *pName
            nullptr                                                     // const VkSpecializationInfo                    *pSpecializationInfo
        }
    };

    VkVertexInputBindingDescription vertexInputBindingDescription;
    vertexInputBindingDescription.binding = 0;
    vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBindingDescription.stride = sizeof (float) * 5;

    VkVertexInputAttributeDescription vertexInputAttributeDescription [2] = {};
    vertexInputAttributeDescription[0].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescription[0].location = 0;
    vertexInputAttributeDescription[0].offset = 0;

    vertexInputAttributeDescription[1].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescription[1].location = 1;
    vertexInputAttributeDescription[1].offset = sizeof (float) * 3;

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,    // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineVertexInputStateCreateFlags          flags;
        1,                                                            // uint32_t                                       vertexBindingDescriptionCount
        &vertexInputBindingDescription,                               // const VkVertexInputBindingDescription         *pVertexBindingDescriptions
        std::extent<decltype(vertexInputAttributeDescription)>::value,// uint32_t                                       vertexAttributeDescriptionCount
        vertexInputAttributeDescription                               // const VkVertexInputAttributeDescription       *pVertexAttributeDescriptions
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,  // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineInputAssemblyStateCreateFlags        flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                          // VkPrimitiveTopology                            topology
        VK_FALSE                                                      // VkBool32                                       primitiveRestartEnable
    };

    VkViewport viewport = {
        0.0f,                                                         // float                                          x
        0.0f,                                                         // float                                          y
        300.0f,                                                       // float                                          width
        300.0f,                                                       // float                                          height
        0.0f,                                                         // float                                          minDepth
        1.0f                                                          // float                                          maxDepth
    };

    VkRect2D scissor = {
        {                                                             // VkOffset2D                                     offset
            0,                                                            // int32_t                                        x
            0                                                             // int32_t                                        y
        },
        {                                                             // VkExtent2D                                     extent
            300,                                                          // int32_t                                        width
            300                                                           // int32_t                                        height
        }
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,        // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineViewportStateCreateFlags             flags
        1,                                                            // uint32_t                                       viewportCount
        &viewport,                                                    // const VkViewport                              *pViewports
        1,                                                            // uint32_t                                       scissorCount
        &scissor                                                      // const VkRect2D                                *pScissors
    };

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,   // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineRasterizationStateCreateFlags        flags
        VK_FALSE,                                                     // VkBool32                                       depthClampEnable
        VK_FALSE,                                                     // VkBool32                                       rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,                                         // VkPolygonMode                                  polygonMode
        VK_CULL_MODE_BACK_BIT,                                        // VkCullModeFlags                                cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                              // VkFrontFace                                    frontFace
        VK_FALSE,                                                     // VkBool32                                       depthBiasEnable
        0.0f,                                                         // float                                          depthBiasConstantFactor
        0.0f,                                                         // float                                          depthBiasClamp
        0.0f,                                                         // float                                          depthBiasSlopeFactor
        1.0f                                                          // float                                          lineWidth
    };

    VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,     // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineMultisampleStateCreateFlags          flags
        VK_SAMPLE_COUNT_1_BIT,                                        // VkSampleCountFlagBits                          rasterizationSamples
        VK_FALSE,                                                     // VkBool32                                       sampleShadingEnable
        1.0f,                                                         // float                                          minSampleShading
        nullptr,                                                      // const VkSampleMask                            *pSampleMask
        VK_FALSE,                                                     // VkBool32                                       alphaToCoverageEnable
        VK_FALSE                                                      // VkBool32                                       alphaToOneEnable
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
        VK_FALSE,                                                     // VkBool32                                       blendEnable
        VK_BLEND_FACTOR_ONE,                                          // VkBlendFactor                                  srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,                                         // VkBlendFactor                                  dstColorBlendFactor
        VK_BLEND_OP_ADD,                                              // VkBlendOp                                      colorBlendOp
        VK_BLEND_FACTOR_ONE,                                          // VkBlendFactor                                  srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,                                         // VkBlendFactor                                  dstAlphaBlendFactor
        VK_BLEND_OP_ADD,                                              // VkBlendOp                                      alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |         // VkColorComponentFlags                          colorWriteMask
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,     // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineColorBlendStateCreateFlags           flags
        VK_FALSE,                                                     // VkBool32                                       logicOpEnable
        VK_LOGIC_OP_COPY,                                             // VkLogicOp                                      logicOp
        1,                                                            // uint32_t                                       attachmentCount
        &color_blend_attachment_state,                                // const VkPipelineColorBlendAttachmentState     *pAttachments
        { 0.0f, 0.0f, 0.0f, 0.0f }                                    // float                                          blendConstants[4]
    };

    VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_VERTEX_BIT, // VkShaderStageFlags    stageFlags;
        0,                          // uint32_t              offset;
        sizeof(VSPushConstants)     // uint32_t              size;
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,  // VkStructureType                sType
        nullptr,                                        // const void                    *pNext
        0,                                              // VkPipelineLayoutCreateFlags    flags
        0,                                              // uint32_t                       setLayoutCount
        nullptr,                                        // const VkDescriptorSetLayout   *pSetLayouts
        1,                                              // uint32_t                       pushConstantRangeCount
        &pushConstantRange                              // const VkPushConstantRange     *pPushConstantRanges
    };

    Assert(vkCreatePipelineLayout(_device, &layout_create_info, nullptr, &_pipelineLayout) == VK_SUCCESS);

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,              // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineCreateFlags                          flags
        static_cast<uint32_t>(shader_stage_create_infos.size()),      // uint32_t                                       stageCount
        &shader_stage_create_infos[0],                                // const VkPipelineShaderStageCreateInfo         *pStages
        &vertex_input_state_create_info,                              // const VkPipelineVertexInputStateCreateInfo    *pVertexInputState;
        &input_assembly_state_create_info,                            // const VkPipelineInputAssemblyStateCreateInfo  *pInputAssemblyState
        nullptr,                                                      // const VkPipelineTessellationStateCreateInfo   *pTessellationState
        &viewport_state_create_info,                                  // const VkPipelineViewportStateCreateInfo       *pViewportState
        &rasterization_state_create_info,                             // const VkPipelineRasterizationStateCreateInfo  *pRasterizationState
        &multisample_state_create_info,                               // const VkPipelineMultisampleStateCreateInfo    *pMultisampleState
        nullptr,                                                      // const VkPipelineDepthStencilStateCreateInfo   *pDepthStencilState
        &color_blend_state_create_info,                               // const VkPipelineColorBlendStateCreateInfo     *pColorBlendState
        nullptr,                                                      // const VkPipelineDynamicStateCreateInfo        *pDynamicState
        _pipelineLayout,                                              // VkPipelineLayout                               layout
        _renderPass,                                                  // VkRenderPass                                   renderPass
        0,                                                            // uint32_t                                       subpass
        VK_NULL_HANDLE,                                               // VkPipeline                                     basePipelineHandle
        -1                                                            // int32_t                                        basePipelineIndex
    };

    Assert(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &_pipeline) == VK_SUCCESS);

    vkDestroyShaderModule(_device, fragModule, nullptr);
    vkDestroyShaderModule(_device, vertModule, nullptr);
}

namespace
{
    struct MemoryTypeInfo
    {
        bool deviceLocal = false;
        bool hostVisible = false;
        bool hostCoherent = false;
        bool hostCached = false;
        bool lazilyAllocated = false;

        struct Heap
        {
            uint64_t size = 0;
            bool deviceLocal = false;
        };

        Heap heap;
        int index;
    };

    std::vector<MemoryTypeInfo> enumerateHeaps (VkPhysicalDevice device)
    {
        VkPhysicalDeviceMemoryProperties memoryProperties = {};
        vkGetPhysicalDeviceMemoryProperties (device, &memoryProperties);

        std::vector<MemoryTypeInfo::Heap> heaps;

        for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i) {
            MemoryTypeInfo::Heap info;
            info.size = memoryProperties.memoryHeaps [i].size;
            info.deviceLocal = (memoryProperties.memoryHeaps [i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;

            heaps.push_back (info);
        }

        std::vector<MemoryTypeInfo> result;

        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
            MemoryTypeInfo typeInfo;

            typeInfo.deviceLocal = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
            typeInfo.hostVisible = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
            typeInfo.hostCoherent = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
            typeInfo.hostCached = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;
            typeInfo.lazilyAllocated = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0;

            typeInfo.heap = heaps [memoryProperties.memoryTypes [i].heapIndex];

            typeInfo.index = static_cast<int> (i);

            result.push_back (typeInfo);
        }

        return result;
    }

    VkDeviceMemory AllocateMemory (const std::vector<MemoryTypeInfo>& memoryInfos,
                                   VkDevice device, const int size)
    {
        // We take the first HOST_VISIBLE memory
        for (auto& memoryInfo : memoryInfos) {
            if (memoryInfo.hostVisible) {
                VkMemoryAllocateInfo memoryAllocateInfo = {};
                memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                memoryAllocateInfo.memoryTypeIndex = memoryInfo.index;
                memoryAllocateInfo.allocationSize = size;

                VkDeviceMemory deviceMemory;
                vkAllocateMemory (device, &memoryAllocateInfo, nullptr,
                                  &deviceMemory);
                return deviceMemory;
            }
        }

        return VK_NULL_HANDLE;
    }

    VkBuffer AllocateBuffer (VkDevice device, const int size,
                             const VkBufferUsageFlagBits bits)
    {
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = static_cast<uint32_t> (size);
        bufferCreateInfo.usage = bits;

        VkBuffer result;
        vkCreateBuffer (device, &bufferCreateInfo, nullptr, &result);
        return result;
    }
}

void VulkanRenderer::createMeshBuffers()
{
    struct Vertex
    {
        float position[3];
        float uv[2];
    };

    static const Vertex vertices[4] = {
        // Upper Left
        { { -1.0f, 1.0f, 0 },{ 0, 0 } },
        // Upper Right
        { { 1.0f, 1.0f, 0 },{ 1, 0 } },
        // Bottom right
        { { 1.0f, -1.0f, 0 },{ 1, 1 } },
        // Bottom left
        { { -1.0f, -1.0f, 0 },{ 0, 1 } }
    };

    static const int indices[6] = {
        0, 1, 2, 2, 3, 0
    };

    auto memoryHeaps = enumerateHeaps(_physicalDevice);
    _deviceMemory = AllocateMemory(memoryHeaps, _device,
                                    640 << 10 /* 640 KiB ought to be enough for anybody */);
    _vertexBuffer = AllocateBuffer(_device, sizeof (vertices),
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    vkBindBufferMemory (_device, _vertexBuffer, _deviceMemory, 0);

    _indexBuffer = AllocateBuffer (_device, sizeof (indices),
                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    vkBindBufferMemory (_device, _indexBuffer, _deviceMemory,
                        256 /* somewhere behind the vertex buffer */);

    void* mapping = nullptr;
    vkMapMemory (_device, _deviceMemory, 0, VK_WHOLE_SIZE,
                    0, &mapping);
    ::memcpy(mapping, vertices, sizeof (vertices));
    ::memcpy(static_cast<uint8_t*> (mapping) + 256 /* same offset as above */,
                indices, sizeof (indices));
    vkUnmapMemory (_device, _deviceMemory);
}
