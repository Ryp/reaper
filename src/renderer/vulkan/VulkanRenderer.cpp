////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VulkanRenderer.h"

#include <cstring>

#include "PresentationSurface.h"

using namespace vk;

static bool checkExtensionAvailability(const char *extension_name, const std::vector<VkExtensionProperties> &available_extensions)
{
    for (size_t i = 0; i < available_extensions.size(); ++i)
    {
        if (std::strcmp(available_extensions[i].extensionName, extension_name) == 0)
            return true;
    }
    return false;
}

static bool checkPhysicalDeviceProperties(VkPhysicalDevice physical_device, VkSurfaceKHR presentationSurface, uint32_t& queue_family_index, uint32_t& selected_present_queue_family_index)
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

void VulkanRenderer::startup(Window* window)
{
    _vulkanLib = dynlib::load(REAPER_VK_LIB_NAME);
    _instance = VK_NULL_HANDLE;
    _physicalDevice = VK_NULL_HANDLE;
    _imageAvailableSemaphore = VK_NULL_HANDLE;
    _renderingFinishedSemaphore = VK_NULL_HANDLE;
    _presentationSurface = VK_NULL_HANDLE;
    _presentationQueue = VK_NULL_HANDLE;
    _swapChain = VK_NULL_HANDLE;
    _device = VK_NULL_HANDLE;

    _presentCmdPool = VK_NULL_HANDLE;
    _presentCmdBuffers.clear();
    _presentQueueIndex = UINT32_MAX;

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

    VkInstanceCreateInfo instance_create_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,     // VkStructureType            sType
        nullptr,                                    // const void*                pNext
        0,                                          // VkInstanceCreateFlags      flags
        &application_info,                          // const VkApplicationInfo   *pApplicationInfo
        0,                                          // uint32_t                   enabledLayerCount
        nullptr,                                    // const char * const        *ppEnabledLayerNames
        static_cast<uint32_t>(extensions.size()),   // uint32_t                   enabledExtensionCount
        &extensions[0]                              // const char * const        *ppEnabledExtensionNames
    };

    Assert(vkCreateInstance(&instance_create_info, nullptr, &_instance) == VK_SUCCESS, "cannot create Vulkan instance");

    #define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func) {                           \
        func = (PFN_##func)vkGetInstanceProcAddr(_instance, #func);             \
        Assert(func != nullptr, "could not load instance level vk function"); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"

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
    _presentQueueIndex = selected_present_queue_family_index;

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

    VkDeviceCreateInfo device_create_info = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,           // VkStructureType                    sType
        nullptr,                                        // const void                        *pNext
        0,                                              // VkDeviceCreateFlags                flags
        static_cast<uint32_t>(queue_create_infos.size()),// uint32_t                           queueCreateInfoCount
        &queue_create_infos[0],                          // const VkDeviceQueueCreateInfo     *pQueueCreateInfos
        0,                                              // uint32_t                           enabledLayerCount
        nullptr,                                        // const char * const                *ppEnabledLayerNames
        static_cast<uint32_t>(device_extensions.size()),// uint32_t                           enabledExtensionCount
        &device_extensions[0],                          // const char * const                *ppEnabledExtensionNames
        nullptr                                         // const VkPhysicalDeviceFeatures    *pEnabledFeatures
    };

    Assert(vkCreateDevice(_physicalDevice, &device_create_info, nullptr, &_device) == VK_SUCCESS, "could not create Vulkan device");

    #define REAPER_VK_DEVICE_LEVEL_FUNCTION(func) {                             \
        func = (PFN_##func)vkGetDeviceProcAddr(_device, #func);                 \
        Assert(func != nullptr, "could not load device level vk function"); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    VkQueue vkGraphicsQueue;
    vkGetDeviceQueue(_device, selected_queue_family_index, 0, &vkGraphicsQueue);

    vkGetDeviceQueue(_device, selected_present_queue_family_index, 0, &_presentationQueue);

    createSemaphores();
    createSwapChain();
    createCommandBuffers();
}

void VulkanRenderer::shutdown()
{
    Assert(vkDeviceWaitIdle(_device) == VK_SUCCESS);
    vkFreeCommandBuffers(_device, _presentCmdPool, static_cast<uint32_t>(_presentCmdBuffers.size()), &_presentCmdBuffers[0]);
    vkDestroyCommandPool(_device, _presentCmdPool, nullptr);

    Assert(vkDeviceWaitIdle(_device) == VK_SUCCESS);
    vkDestroySemaphore(_device, _imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(_device, _renderingFinishedSemaphore, nullptr);
    vkDestroySwapchainKHR(_device, _swapChain, nullptr);
    vkDestroyDevice(_device, nullptr);
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
        &_presentCmdBuffers[image_index],   // const VkCommandBuffer       *pCommandBuffers
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
    }
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
        VkExtent2D swap_chain_extent = { 640, 480 };
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
}

void VulkanRenderer::createCommandBuffers()
{
    VkCommandPoolCreateInfo cmd_pool_create_info = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,     // VkStructureType              sType
        nullptr,                                        // const void*                  pNext
        0,                                              // VkCommandPoolCreateFlags     flags
        _presentQueueIndex                              // uint32_t                     queueFamilyIndex
    };

    Assert(vkCreateCommandPool(_device, &cmd_pool_create_info, nullptr, &_presentCmdPool) == VK_SUCCESS);

    uint32_t image_count = 0;
    Assert(vkGetSwapchainImagesKHR(_device, _swapChain, &image_count, nullptr) == VK_SUCCESS);
    Assert(image_count > 0);

    _presentCmdBuffers.resize( image_count );

    VkCommandBufferAllocateInfo cmd_buffer_allocate_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType              sType
        nullptr,                                        // const void*                  pNext
        _presentCmdPool,                                // VkCommandPool                commandPool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel         level
        image_count                                     // uint32_t                     bufferCount
    };

    Assert(vkAllocateCommandBuffers(_device, &cmd_buffer_allocate_info, &_presentCmdBuffers[0]) == VK_SUCCESS);

    std::vector<VkImage> swap_chain_images(image_count);
    Assert(vkGetSwapchainImagesKHR(_device, _swapChain, &image_count, &swap_chain_images[0]) == VK_SUCCESS);

    VkCommandBufferBeginInfo cmd_buffer_begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureType                        sType
        nullptr,                                      // const void                            *pNext
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // VkCommandBufferUsageFlags              flags
        nullptr                                       // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
    };

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
            swap_chain_images[i],                       // VkImage                                image
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
            swap_chain_images[i],                       // VkImage                                image
            image_subresource_range                     // VkImageSubresourceRange                subresourceRange
        };

        Assert(vkBeginCommandBuffer(_presentCmdBuffers[i], &cmd_buffer_begin_info) == VK_SUCCESS);
        vkCmdPipelineBarrier(_presentCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_present_to_clear);

        vkCmdClearColorImage(_presentCmdBuffers[i], swap_chain_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_subresource_range );

        vkCmdPipelineBarrier(_presentCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_clear_to_present);
        Assert(vkEndCommandBuffer(_presentCmdBuffers[i]) == VK_SUCCESS);
    }
}
