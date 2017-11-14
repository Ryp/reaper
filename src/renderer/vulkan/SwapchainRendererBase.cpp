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

namespace Reaper
{
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
} // namespace

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
{}

VulkanBackend::VulkanBackend()
    : vulkanLib(nullptr)
    , instance(VK_NULL_HANDLE)
    , physicalDevice(VK_NULL_HANDLE)
    , physicalDeviceInfo({0, 0, {}})
    , device(VK_NULL_HANDLE)
    , deviceInfo({VK_NULL_HANDLE, VK_NULL_HANDLE})
    , presentInfo()
    , debugCallback(VK_NULL_HANDLE)
{}

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
} // namespace

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
} // namespace

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
} // namespace Reaper
