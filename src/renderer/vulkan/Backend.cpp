////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Backend.h"

// TODO use Pimpl to prevent name clashes between Xlib and fmt and move this include forward
#include "common/Log.h"

#include "core/Profile.h"

#include "Display.h"
#include "Swapchain.h"

#include <cstring>
#include <iostream>

#include "PresentationSurface.h"
#include "renderer/window/Window.h"

#ifndef REAPER_VK_LIB_NAME
#    error
#endif

// Version of the API to query when loading vulkan symbols
#define REAPER_VK_API_VERSION VK_MAKE_VERSION(1, 2, 0)

// Decide which swapchain extension to use
#if defined(VK_USE_PLATFORM_WIN32_KHR)
#    define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#    define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#    define REAPER_VK_SWAPCHAIN_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif

namespace Reaper
{
namespace
{
    VkDescriptorPool create_global_descriptor_pool(ReaperRoot& root, VulkanBackend& backend)
    {
        // Create descriptor pool
        // FIXME Sizes are arbitrary for now, as long as everything fits
        constexpr u32                     MaxDescriptorSets = 100;
        std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {{VK_DESCRIPTOR_TYPE_SAMPLER, 16},
                                                                 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256},
                                                                 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
                                                                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
                                                                 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32}};

        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                               nullptr,
                                               VK_FLAGS_NONE,
                                               MaxDescriptorSets,
                                               static_cast<uint32_t>(descriptorPoolSizes.size()),
                                               descriptorPoolSizes.data()};

        VkDescriptorPool pool = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorPool(backend.device, &poolInfo, nullptr, &pool) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor pool with handle: {}", static_cast<void*>(pool));

        return pool;
    }
} // namespace

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

void vulkan_choose_physical_device(ReaperRoot&                     root,
                                   VulkanBackend&                  backend,
                                   const std::vector<const char*>& device_extensions,
                                   PhysicalDeviceInfo&             physicalDeviceInfo);

void vulkan_create_logical_device(ReaperRoot&                     root,
                                  VulkanBackend&                  backend,
                                  const std::vector<const char*>& device_extensions);

VulkanBackend::VulkanBackend()
    : vulkanLib(nullptr)
    , instance(VK_NULL_HANDLE)
    , physicalDevice(VK_NULL_HANDLE)
    , physicalDeviceInfo({0, 0, {}})
    , device(VK_NULL_HANDLE)
    , deviceInfo({VK_NULL_HANDLE, VK_NULL_HANDLE})
    , presentInfo({})
    , debugMessenger(VK_NULL_HANDLE)
    , mustTransitionSwapchain(false)
{
    options.freeze_culling = false;
    options.use_compacted_draw = true;
}

void create_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE("Vulkan", MP_RED1);
    log_info(root, "vulkan: creating backend");

    log_debug(root, "vulkan: loading {}", REAPER_VK_LIB_NAME);
    backend.vulkanLib = dynlib::load(REAPER_VK_LIB_NAME);

    vulkan_load_exported_functions(backend.vulkanLib);
    vulkan_load_global_level_functions();

    std::vector<const char*> instanceExtensions = {VK_KHR_SURFACE_EXTENSION_NAME, REAPER_VK_SWAPCHAIN_EXTENSION_NAME};

#if REAPER_DEBUG
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    log_info(root, "vulkan: using {} instance level extensions", instanceExtensions.size());
    for (auto& e : instanceExtensions)
        log_debug(root, "- {}", e);

    vulkan_instance_check_extensions(instanceExtensions);

    std::vector<const char*> instanceLayers;

#if REAPER_DEBUG
    instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    log_info(root, "vulkan: using {} instance level layers", instanceLayers.size());
    for (auto& layer : instanceLayers)
        log_debug(root, "- {}", layer);

    vulkan_instance_check_layers(instanceLayers);

    const u32 appVersion = VK_MAKE_VERSION(REAPER_VERSION_MAJOR, REAPER_VERSION_MINOR, REAPER_VERSION_PATCH);
    const u32 engineVersion = appVersion;
    const u32 vulkanVersion = REAPER_VK_API_VERSION;

    VkApplicationInfo application_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType  sType
        nullptr,                            // const void*      pNext
        "MyGame",                           // const char*      pApplicationName
        appVersion,                         // uint32_t         applicationVersion
        "Reaper",                           // const char*      pEngineName
        engineVersion,                      // uint32_t         engineVersion
        vulkanVersion                       // uint32_t         apiVersion
    };

    VkInstanceCreateInfo instance_create_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,      // VkStructureType            sType
        nullptr,                                     // const void*                pNext
        0,                                           // VkInstanceCreateFlags      flags
        &application_info,                           // const VkApplicationInfo   *pApplicationInfo
        static_cast<u32>(instanceLayers.size()),     // uint32_t                   enabledLayerCount
        instanceLayers.data(),                       // const char * const        *ppEnabledLayerNames
        static_cast<u32>(instanceExtensions.size()), // uint32_t                   enabledExtensionCount
        instanceExtensions.data(),                   // const char * const        *ppEnabledExtensionNames
    };

    Assert(vkCreateInstance(&instance_create_info, nullptr, &backend.instance) == VK_SUCCESS,
           "cannot create Vulkan instance");

    vulkan_load_instance_level_functions(backend.instance);

#if REAPER_DEBUG
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

    std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
        VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    };

    log_debug(root, "vulkan: choosing physical device");
    vulkan_choose_physical_device(root, backend, device_extensions, backend.physicalDeviceInfo);

    log_debug(root, "vulkan: creating logical device");
    vulkan_create_logical_device(root, backend, device_extensions);

    log_debug(root, "vulkan: create global descriptor pool");
    backend.global_descriptor_pool = create_global_descriptor_pool(root, backend);

    log_debug(root, "vulkan: create gpu memory allocator");
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = backend.physicalDevice;
    allocatorInfo.device = backend.device;
    allocatorInfo.instance = backend.instance;

    vmaCreateAllocator(&allocatorInfo, &backend.vma_instance);

    SwapchainDescriptor swapchainDesc;
    swapchainDesc.preferredImageCount = 2; // Double buffering
    swapchainDesc.preferredFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    // swapchainDesc.preferredFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    swapchainDesc.preferredExtent = {windowDescriptor.width, windowDescriptor.height};

    configure_vulkan_wm_swapchain(root, backend, swapchainDesc, backend.presentInfo);
    create_vulkan_wm_swapchain(root, backend, backend.presentInfo);

    // create_vulkan_display_swapchain(root, backend);

#if defined(REAPER_USE_MICROPROFILE)
    const u32 node_count = 1; // NOTE: not sure what this is for
    MicroProfileGpuInitVulkan(&backend.device, &backend.physicalDevice, &backend.deviceInfo.graphicsQueue,
                              &backend.physicalDeviceInfo.graphicsQueueFamilyIndex, node_count);
#endif

    log_info(root, "vulkan: ready");
}

void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE("Vulkan", MP_RED1);
    log_info(root, "vulkan: destroying backend");

    log_debug(root, "vulkan: waiting for current work to finish");
    Assert(vkDeviceWaitIdle(backend.device) == VK_SUCCESS);

#if defined(REAPER_USE_MICROPROFILE)
    MicroProfileGpuShutdown();
#endif

    destroy_vulkan_wm_swapchain(root, backend, backend.presentInfo);

    log_debug(root, "vulkan: destroy gpu memory allocator");
    vmaDestroyAllocator(backend.vma_instance);

    log_debug(root, "vulkan: destroy global descriptor pool");
    vkDestroyDescriptorPool(backend.device, backend.global_descriptor_pool, nullptr);

    log_debug(root, "vulkan: destroying logical device");
    vkDestroyDevice(backend.device, nullptr);

    log_debug(root, "vulkan: destroying presentation surface");
    vkDestroySurfaceKHR(backend.instance, backend.presentInfo.surface, nullptr);

    delete root.renderer->window;
    root.renderer->window = nullptr;

#if REAPER_DEBUG
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
        Assert(found, fmt::format("vulkan: extension '{}' not supported by the instance", extensions[i]));
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
        Assert(found, fmt::format("vulkan: layer '{}' not supported by the instance", layers[i]));
    }
}

namespace
{
    VkBool32 VKAPI_PTR debugReportCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                           VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
                                           const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                           void*                                       pUserData)
    {
        ReaperRoot* root = static_cast<ReaperRoot*>(pUserData);
        Assert(root != nullptr);

        log_debug(*root,
                  "vulkan debug message: {} ({}): {}",
                  pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "unnamed",
                  pCallbackData->messageIdNumber,
                  pCallbackData->pMessage);

        u32 queueCount = pCallbackData->queueLabelCount;
        if (queueCount > 0)
        {
            log_debug(*root, "queues labels:");
            for (u32 i = 0; i < queueCount; i++)
            {
                const VkDebugUtilsLabelEXT& queueInfo = pCallbackData->pQueueLabels[i];
                const char*                 label = queueInfo.pLabelName;
                log_debug(*root, "- {}", label ? label : "unnamed");
            }
        }

        u32 cmdBufferCount = pCallbackData->cmdBufLabelCount;
        if (cmdBufferCount > 0)
        {
            log_debug(*root, "command buffer labels:");
            for (u32 i = 0; i < cmdBufferCount; i++)
            {
                const VkDebugUtilsLabelEXT& cmdBufferInfo = pCallbackData->pCmdBufLabels[i];
                const char*                 label = cmdBufferInfo.pLabelName;
                log_debug(*root, "- {}", label ? label : "unnamed");
            }
        }

        u32 objetCount = pCallbackData->objectCount;
        if (objetCount > 0)
        {
            log_debug(*root, "oject info:");
            for (u32 i = 0; i < objetCount; i++)
            {
                const VkDebugUtilsObjectNameInfoEXT& objectInfo = pCallbackData->pObjects[i];
                const char*                          label = objectInfo.pObjectName;
                log_debug(*root, "- {}, handle = {}", label ? label : "unnamed", objectInfo.objectHandle);
            }
        }

        Assert(messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT);

        return VK_FALSE;
    }
} // namespace

void vulkan_setup_debug_callback(ReaperRoot& root, VulkanBackend& backend)
{
    VkDebugUtilsMessengerCreateInfoEXT callbackCreateInfo = {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        nullptr,
        0,
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        &debugReportCallback,
        &root};

#if REAPER_DEBUG
    Assert(vkCreateDebugUtilsMessengerEXT(backend.instance, &callbackCreateInfo, nullptr, &backend.debugMessenger)
           == VK_SUCCESS);
#endif
} // namespace Reaper

void vulkan_destroy_debug_callback(VulkanBackend& backend)
{
    Assert(backend.debugMessenger != nullptr);

#if REAPER_DEBUG
    vkDestroyDebugUtilsMessengerEXT(backend.instance, backend.debugMessenger, nullptr);
#endif

    backend.debugMessenger = nullptr;
}

// Move this up
void vulkan_device_check_extensions(const std::vector<const char*>& extensions, VkPhysicalDevice physicalDevice)
{
    uint32_t extensions_count = 0;
    Assert(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensions_count, nullptr) == VK_SUCCESS);
    Assert(extensions_count > 0);

    std::vector<VkExtensionProperties> available_extensions(extensions_count);
    Assert(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensions_count,
                                                (extensions_count > 0 ? &available_extensions[0] : nullptr))
           == VK_SUCCESS);

    for (size_t i = 0; i < extensions.size(); ++i)
    {
        bool found = false;
        for (size_t j = 0; j < available_extensions.size(); ++j)
        {
            if (std::strcmp(available_extensions[j].extensionName, extensions[i]) == 0)
                found = true;
        }
        Assert(found, fmt::format("vulkan: extension '{}' not supported by the device", extensions[i]));
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

    VkPhysicalDeviceProperties2 device_properties2 = {};
    device_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    vkGetPhysicalDeviceProperties2(physical_device, &device_properties2);
    VkPhysicalDeviceProperties& device_properties = device_properties2.properties;

    // NOTE:
    // For some reason the validation layer barks at us if we don't initialize sType to this value.
    // This shouldn't be the case since vkGetPhysicalDeviceFeatures2 should overwrite it.
    VkPhysicalDeviceFeatures2 device_features2 = {};
    device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    // NOTE:
    // Even if VkPhysicalDeviceVulkan12Features is used during device creation,
    // Vulkan 1.2 does NOT force vkGetPhysicalDeviceFeatures2 to include it in the pNext chain.
    vkGetPhysicalDeviceFeatures2(physical_device, &device_features2);

    Assert(device_properties.apiVersion >= REAPER_VK_API_VERSION);
    Assert(device_properties.limits.maxImageDimension2D >= 4096);
    Assert(device_features2.features.shaderClipDistance == VK_TRUE); // This is just checked, not enabled

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

void vulkan_choose_physical_device(ReaperRoot&                     root,
                                   VulkanBackend&                  backend,
                                   const std::vector<const char*>& device_extensions,
                                   PhysicalDeviceInfo&             physicalDeviceInfo)
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

    VkPhysicalDevice chosenPhysicalDevice = VK_NULL_HANDLE;
    for (auto& device : availableDevices)
    {
        if (vulkan_check_physical_device(root.renderer->window,
                                         device,
                                         backend.presentInfo.surface,
                                         device_extensions,
                                         selected_queue_family_index,
                                         selected_present_queue_family_index))
        {
            chosenPhysicalDevice = device;
            break;
        }
    }

    Assert(chosenPhysicalDevice != VK_NULL_HANDLE, "could not select physical device based on the chosen properties");

    physicalDeviceInfo.graphicsQueueFamilyIndex = selected_queue_family_index;
    physicalDeviceInfo.presentQueueFamilyIndex = selected_present_queue_family_index;

    vkGetPhysicalDeviceMemoryProperties(chosenPhysicalDevice, &physicalDeviceInfo.memory);

    // re-fetch device infos TODO avoid
    VkPhysicalDeviceDriverProperties deviceDriverProperties;
    deviceDriverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    deviceDriverProperties.pNext = nullptr;

    VkPhysicalDeviceSubgroupProperties subgroupProperties;
    subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    subgroupProperties.pNext = &deviceDriverProperties;

    VkPhysicalDeviceProperties2 physicalDeviceProperties2;
    physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physicalDeviceProperties2.pNext = &subgroupProperties;

    vkGetPhysicalDeviceProperties2(chosenPhysicalDevice, &physicalDeviceProperties2);

    Assert(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT);
    Assert(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
    Assert(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT);
    Assert(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT);
    // VK_SUBGROUP_FEATURE_BASIC_BIT = 0x00000001,
    // VK_SUBGROUP_FEATURE_VOTE_BIT = 0x00000002,
    // VK_SUBGROUP_FEATURE_ARITHMETIC_BIT = 0x00000004,
    // VK_SUBGROUP_FEATURE_BALLOT_BIT = 0x00000008,
    // VK_SUBGROUP_FEATURE_SHUFFLE_BIT = 0x00000010,
    // VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT = 0x00000020,
    // VK_SUBGROUP_FEATURE_CLUSTERED_BIT = 0x00000040,
    // VK_SUBGROUP_FEATURE_QUAD_BIT = 0x00000080,
    // VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV = 0x00000100,

    const VkPhysicalDeviceProperties physicalDeviceProperties = physicalDeviceProperties2.properties;

    log_info(root, "vulkan: selecting device '{}'", physicalDeviceProperties.deviceName);
    log_debug(root, "- type = {}", vulkan_physical_device_type_name(physicalDeviceProperties.deviceType));

    uint32_t apiVersion = physicalDeviceProperties.apiVersion;
    uint32_t driverVersion = physicalDeviceProperties.driverVersion;
    log_debug(root,
              "- api version = {}.{}.{}",
              VK_VERSION_MAJOR(apiVersion),
              VK_VERSION_MINOR(apiVersion),
              VK_VERSION_PATCH(apiVersion));

    log_debug(root, "- driver '{}' version = {}.{}.{}", deviceDriverProperties.driverName,
              VK_VERSION_MAJOR(driverVersion), VK_VERSION_MINOR(driverVersion), VK_VERSION_PATCH(driverVersion));

    log_debug(root, "- driver info '{}' conformance version = {}.{}.{}.{}", deviceDriverProperties.driverInfo,
              deviceDriverProperties.conformanceVersion.major, deviceDriverProperties.conformanceVersion.minor,
              deviceDriverProperties.conformanceVersion.subminor, deviceDriverProperties.conformanceVersion.patch);

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
    backend.physicalDeviceProperties = physicalDeviceProperties;
}

void vulkan_create_logical_device(ReaperRoot&                     root,
                                  VulkanBackend&                  backend,
                                  const std::vector<const char*>& device_extensions)
{
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::vector<float>                   queue_priorities = {1.0f};

    queue_create_infos.push_back({
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,          // VkStructureType              sType
        nullptr,                                             // const void                  *pNext
        0,                                                   // VkDeviceQueueCreateFlags     flags
        backend.physicalDeviceInfo.graphicsQueueFamilyIndex, // uint32_t                     queueFamilyIndex
        static_cast<uint32_t>(queue_priorities.size()),      // uint32_t                     queueCount
        &queue_priorities[0]                                 // const float                 *pQueuePriorities
    });

    if (backend.physicalDeviceInfo.graphicsQueueFamilyIndex != backend.physicalDeviceInfo.presentQueueFamilyIndex)
    {
        queue_create_infos.push_back({
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,         // VkStructureType              sType
            nullptr,                                            // const void                  *pNext
            0,                                                  // VkDeviceQueueCreateFlags     flags
            backend.physicalDeviceInfo.presentQueueFamilyIndex, // uint32_t                     queueFamilyIndex
            static_cast<uint32_t>(queue_priorities.size()),     // uint32_t                     queueCount
            &queue_priorities[0]                                // const float                 *pQueuePriorities
        });
    }

    Assert(!queue_create_infos.empty());
    Assert(!queue_priorities.empty());
    Assert(queue_priorities.size() == queue_create_infos.size());

    uint32_t queueCreateCount = static_cast<uint32_t>(queue_create_infos.size());

    log_info(root, "vulkan: using {} device level extensions", device_extensions.size());
    for (auto& e : device_extensions)
        log_debug(root, "- {}", e);

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.multiDrawIndirect = true;
    deviceFeatures.drawIndirectFirstInstance = true;

    VkPhysicalDeviceVulkan12Features device_features_1_2 = {};
    device_features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    device_features_1_2.pNext = nullptr;
    device_features_1_2.drawIndirectCount = VK_TRUE;
    device_features_1_2.imagelessFramebuffer = VK_TRUE;
    device_features_1_2.separateDepthStencilLayouts = VK_TRUE;
    device_features_1_2.descriptorIndexing = VK_TRUE;
    device_features_1_2.runtimeDescriptorArray = VK_TRUE;
    device_features_1_2.descriptorBindingPartiallyBound = VK_TRUE;
    device_features_1_2.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &device_features_1_2;
    deviceFeatures2.features = deviceFeatures;

    VkDeviceCreateInfo device_create_info = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,       // VkStructureType                    sType
        &deviceFeatures2,                           // const void                        *pNext
        0,                                          // VkDeviceCreateFlags                flags
        queueCreateCount,                           // uint32_t                           queueCreateInfoCount
        &queue_create_infos[0],                     // const VkDeviceQueueCreateInfo     *pQueueCreateInfos
        0,                                          // uint32_t                           enabledLayerCount
        nullptr,                                    // const char * const                *ppEnabledLayerNames
        static_cast<u32>(device_extensions.size()), // uint32_t                           enabledExtensionCount
        device_extensions.data(),                   // const char * const *ppEnabledExtensionNames
        nullptr                                     // const VkPhysicalDeviceFeatures    *pEnabledFeatures
    };

    Assert(vkCreateDevice(backend.physicalDevice, &device_create_info, nullptr, &backend.device) == VK_SUCCESS,
           "could not create Vulkan device");

    vulkan_load_device_level_functions(backend.device);

    vkGetDeviceQueue(backend.device, backend.physicalDeviceInfo.graphicsQueueFamilyIndex, 0,
                     &backend.deviceInfo.graphicsQueue);
    vkGetDeviceQueue(backend.device, backend.physicalDeviceInfo.presentQueueFamilyIndex, 0,
                     &backend.deviceInfo.presentQueue);
}
} // namespace Reaper
