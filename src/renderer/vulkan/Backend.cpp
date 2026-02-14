////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Backend.h"

// TODO use Pimpl to prevent name clashes between Xlib and fmt and move this include forward
#include "common/Log.h"

#include "profiling/Scope.h"
#include <core/Assert.h>
#include <core/Platform.h>
#include <core/Version.h>

#include "Debug.h"
#include "DebugMessageCallback.h"
#include "Display.h"
#include "Semaphore.h"
#include "Swapchain.h"

#include <cstring>
#include <iostream>

#include "PresentationSurface.h"
#include "api/AssertHelper.h"
#include "api/VulkanHook.h"
#include "api/VulkanStringConversion.h"
#include "renderer/window/Window.h"

#include "BackendResources.h"

#include <backends/imgui_impl_vulkan.h>

#ifndef REAPER_VK_LIB_NAME
#    error
#endif

// Version of the API to query when loading vulkan symbols
#define REAPER_VK_API_VERSION VK_MAKE_VERSION(1, 4, 0)

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
    void vulkan_instance_check_extensions(std::span<const char*> checked_extensions)
    {
        if (checked_extensions.empty())
            return;

        uint32_t extensions_count = 0;
        AssertVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, nullptr));

        // NOTE: Vulkan spec states that it's safe to pass zero as a count with an invalid pointer
        std::vector<VkExtensionProperties> supported_extensions(extensions_count);
        AssertVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, supported_extensions.data()));

        for (auto checked_extension : checked_extensions)
        {
            bool found = false;

            for (auto supported_extension : supported_extensions)
            {
                if (std::strcmp(supported_extension.extensionName, checked_extension) == 0)
                    found = true;
            }
            Assert(found, fmt::format("vulkan: extension '{}' not supported by the instance", checked_extension));
        }
    }

    void vulkan_instance_check_layers(std::span<const char*> checked_layer_names)
    {
        if (checked_layer_names.empty())
            return;

        uint32_t layers_count = 0;
        AssertVk(vkEnumerateInstanceLayerProperties(&layers_count, nullptr));
        Assert(layers_count > 0);

        // NOTE: Vulkan spec states that it's safe to pass zero as a count with an invalid pointer
        std::vector<VkLayerProperties> supported_layers(layers_count);
        AssertVk(vkEnumerateInstanceLayerProperties(&layers_count, supported_layers.data()));

        for (auto checked_layer_name : checked_layer_names)
        {
            bool found = false;
            for (auto supported_layer : supported_layers)
            {
                if (std::strcmp(supported_layer.layerName, checked_layer_name) == 0)
                    found = true;
            }
            Assert(found, fmt::format("vulkan: layer '{}' not supported by the instance", checked_layer_name));
        }
    }

    void vulkan_create_logical_device(ReaperRoot& root, VulkanBackend& backend,
                                      std::span<const char*> device_extension_names)
    {
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::vector<float>                   queue_priorities = {1.0f};

        queue_create_infos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FLAGS_NONE,
            .queueFamilyIndex = backend.physical_device.graphics_queue_family_index,
            .queueCount = static_cast<uint32_t>(queue_priorities.size()),
            .pQueuePriorities = queue_priorities.data(),
        });

        if (backend.physical_device.graphics_queue_family_index != backend.physical_device.present_queue_family_index)
        {
            queue_create_infos.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_FLAGS_NONE,
                .queueFamilyIndex = backend.physical_device.present_queue_family_index,
                .queueCount = static_cast<uint32_t>(queue_priorities.size()),
                .pQueuePriorities = queue_priorities.data(),
            });
        }

        Assert(!queue_create_infos.empty());
        Assert(!queue_priorities.empty());
        Assert(queue_priorities.size() == queue_create_infos.size());

        uint32_t queueCreateCount = static_cast<uint32_t>(queue_create_infos.size());

        log_info(root, "vulkan: using {} device level extensions", device_extension_names.size());
        for (auto& e : device_extension_names)
            log_debug(root, "- {}", e);

        VkPhysicalDeviceFeatures2 device_features2 = {};
        device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        device_features2.features.samplerAnisotropy = VK_TRUE;
        device_features2.features.multiDrawIndirect = VK_TRUE;
        device_features2.features.drawIndirectFirstInstance = VK_TRUE;
        device_features2.features.fragmentStoresAndAtomics = VK_TRUE;
        device_features2.features.fillModeNonSolid = VK_TRUE;
        device_features2.features.geometryShader = VK_TRUE;

        VkPhysicalDeviceVulkan12Features features_vk_1_2 = {};
        features_vk_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features_vk_1_2.drawIndirectCount = VK_TRUE;
        features_vk_1_2.imagelessFramebuffer = VK_TRUE;
        features_vk_1_2.separateDepthStencilLayouts = VK_TRUE;
        features_vk_1_2.descriptorIndexing = VK_TRUE;
        features_vk_1_2.runtimeDescriptorArray = VK_TRUE;
        features_vk_1_2.descriptorBindingPartiallyBound = VK_TRUE;
        features_vk_1_2.timelineSemaphore = VK_TRUE;
        features_vk_1_2.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

        VkPhysicalDeviceVulkan13Features features_vk_1_3 = {};
        features_vk_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features_vk_1_3.synchronization2 = VK_TRUE;
        features_vk_1_3.dynamicRendering = VK_TRUE;
        features_vk_1_3.subgroupSizeControl = VK_TRUE;

        VkPhysicalDeviceVulkan14Features features_vk_1_4 = {};
        features_vk_1_4.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
        features_vk_1_4.indexTypeUint8 = VK_TRUE;
        features_vk_1_4.maintenance5 = VK_TRUE;

        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT shader_atomic_features = {};
        shader_atomic_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;

        VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT primitive_restart_features = {};
        primitive_restart_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;
        primitive_restart_features.primitiveTopologyListRestart = VK_TRUE;

        vk_hook(device_features2,
                vk_hook(features_vk_1_2,
                        vk_hook(features_vk_1_3,
                                vk_hook(features_vk_1_4,
                                        vk_hook(shader_atomic_features, vk_hook(primitive_restart_features))))));

        const VkDeviceCreateInfo device_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &device_features2,
            .flags = VK_FLAGS_NONE,
            .queueCreateInfoCount = queueCreateCount,
            .pQueueCreateInfos = &queue_create_infos[0],
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<u32>(device_extension_names.size()),
            .ppEnabledExtensionNames = device_extension_names.data(),
            .pEnabledFeatures = nullptr,
        };

        AssertVk(vkCreateDevice(backend.physical_device.handle, &device_create_info, nullptr, &backend.device));

        VulkanSetDebugName(backend.device, backend.device, "Device");

        vulkan_load_device_level_functions(backend.device);

        vkGetDeviceQueue(backend.device, backend.physical_device.graphics_queue_family_index, 0,
                         &backend.graphics_queue);
        vkGetDeviceQueue(backend.device, backend.physical_device.present_queue_family_index, 0, &backend.present_queue);
    }

    void vulkan_check_physical_device_supported_extensions(
        VkPhysicalDevice physical_device, std::span<const char*> checked_extensions)
    {
        if (checked_extensions.empty())
            return;

        uint32_t extensions_count = 0;
        AssertVk(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensions_count, nullptr));

        // NOTE: Vulkan spec states that it's safe to pass zero as a count with an invalid pointer
        std::vector<VkExtensionProperties> supported_extensions(extensions_count);
        AssertVk(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensions_count,
                                                      supported_extensions.data()));

        for (auto checked_extension : checked_extensions)
        {
            bool found = false;

            for (auto supported_extension : supported_extensions)
            {
                if (std::strcmp(supported_extension.extensionName, checked_extension) == 0)
                    found = true;
            }

            Assert(found, fmt::format("vulkan: extension '{}' not supported by the device", checked_extension));
        }
    }

    bool vulkan_check_physical_device(const PhysicalDeviceInfo& physical_device,
                                      VkPhysicalDevice physical_device_handle, std::span<const char*> extensions)
    {
        // Properties
        Assert(physical_device.properties.apiVersion >= REAPER_VK_API_VERSION, "Unsupported Vulkan version");
        Assert(physical_device.properties_vk_1_1.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT);
        Assert(physical_device.properties_vk_1_1.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
        Assert(physical_device.properties_vk_1_1.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT);
        Assert(physical_device.properties_vk_1_1.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT);

        // FIXME use MinWaveLaneCount from the shader code here
        Assert(physical_device.properties_vk_1_3.minSubgroupSize >= 8);
        Assert(physical_device.properties_vk_1_3.maxSubgroupSize <= 64);

        // Supported Features
        Assert(physical_device.features.samplerAnisotropy == VK_TRUE);
        Assert(physical_device.features.multiDrawIndirect == VK_TRUE);
        Assert(physical_device.features.drawIndirectFirstInstance == VK_TRUE);
        Assert(physical_device.features.fragmentStoresAndAtomics == VK_TRUE);
        Assert(physical_device.features.fillModeNonSolid == VK_TRUE);
        Assert(physical_device.features.geometryShader == VK_TRUE);
        Assert(physical_device.features_vk_1_2.drawIndirectCount == VK_TRUE);
        Assert(physical_device.features_vk_1_2.imagelessFramebuffer == VK_TRUE);
        Assert(physical_device.features_vk_1_2.separateDepthStencilLayouts == VK_TRUE);
        Assert(physical_device.features_vk_1_2.descriptorIndexing == VK_TRUE);
        Assert(physical_device.features_vk_1_2.runtimeDescriptorArray == VK_TRUE);
        Assert(physical_device.features_vk_1_2.descriptorBindingPartiallyBound == VK_TRUE);
        Assert(physical_device.features_vk_1_2.timelineSemaphore == VK_TRUE);
        Assert(physical_device.features_vk_1_2.shaderSampledImageArrayNonUniformIndexing == VK_TRUE);

        Assert(physical_device.features_vk_1_3.synchronization2 == VK_TRUE);
        Assert(physical_device.features_vk_1_3.dynamicRendering == VK_TRUE);
        Assert(physical_device.features_vk_1_3.subgroupSizeControl == VK_TRUE);

        Assert(physical_device.features_vk_1_4.indexTypeUint8 == VK_TRUE);
        Assert(physical_device.features_vk_1_4.maintenance5 == VK_TRUE);
        Assert(physical_device.primitive_restart_features.primitiveTopologyListRestart == VK_TRUE);

        // Supported Queues
        Assert(physical_device.graphics_queue_family_index != UINT32_MAX);
        Assert(physical_device.present_queue_family_index != UINT32_MAX);

        VkMultisamplePropertiesEXT multisample_properties;
        multisample_properties.sType = VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT;
        multisample_properties.pNext = nullptr;
        vkGetPhysicalDeviceMultisamplePropertiesEXT(physical_device_handle, VK_SAMPLE_COUNT_8_BIT,
                                                    &multisample_properties);

        Assert(multisample_properties.maxSampleLocationGridSize.height >= 1);
        Assert(multisample_properties.maxSampleLocationGridSize.width >= 1);

        // Supported Extensions
        vulkan_check_physical_device_supported_extensions(physical_device_handle, extensions);

        return true;
    }

    struct Version
    {
        u32 major;
        u32 minor;
        u32 patch;
    };

    Version vulkan_extract_version(u32 packed_version)
    {
        return Version{
            .major = VK_VERSION_MAJOR(packed_version),
            .minor = VK_VERSION_MINOR(packed_version),
            .patch = VK_VERSION_PATCH(packed_version),
        };
    }

    void vulkan_print_physical_device_debug(ReaperRoot& root, const PhysicalDeviceInfo& physical_device)
    {
        log_info(root, "vulkan: selecting device '{}'", physical_device.properties.deviceName);
        log_debug(root, "- type = {}", vk_to_string(physical_device.properties.deviceType));

        const Version api_version = vulkan_extract_version(physical_device.properties.apiVersion);

        log_debug(root, "- api version = {}.{}.{}", api_version.major, api_version.minor, api_version.patch);

        const VkPhysicalDeviceVulkan12Properties& properties_vk_1_2 = physical_device.properties_vk_1_2;
        const Version driver_version = vulkan_extract_version(physical_device.properties.driverVersion);

        log_debug(root, "- driver '{}' version = {}.{}.{}", properties_vk_1_2.driverName, driver_version.major,
                  driver_version.minor, driver_version.patch);

        log_debug(root, "- driver info '{}' conformance version = {}.{}.{}.{}", properties_vk_1_2.driverInfo,
                  properties_vk_1_2.conformanceVersion.major, properties_vk_1_2.conformanceVersion.minor,
                  properties_vk_1_2.conformanceVersion.subminor, properties_vk_1_2.conformanceVersion.patch);

        log_debug(root,
                  "- memory type count = {}, memory heap count = {}",
                  physical_device.memory_properties.memoryTypeCount,
                  physical_device.memory_properties.memoryHeapCount);

        log_debug(root, "- subgroup min size = {}", physical_device.properties_vk_1_3.minSubgroupSize);
        log_debug(root, "- subgroup max size = {}", physical_device.properties_vk_1_3.maxSubgroupSize);

        std::span<const VkMemoryHeap> memory_heaps(physical_device.memory_properties.memoryHeaps,
                                                   physical_device.memory_properties.memoryHeapCount);

        for (u32 heap_index = 0; heap_index < memory_heaps.size(); heap_index++)
        {
            const VkMemoryHeap& heap = memory_heaps[heap_index];
            log_debug(root, "- heap_index {}: available size = {}, flags = {}", heap_index, heap.size, heap.flags);
        }

        std::span<const VkMemoryType> memory_types(physical_device.memory_properties.memoryTypes,
                                                   physical_device.memory_properties.memoryTypeCount);

        for (auto memory_type : memory_types)
        {
            log_debug(root, "- memory type: heap_index = {}", memory_type.heapIndex);

            VkMemoryPropertyFlags flags = memory_type.propertyFlags;

            for (u32 i = 0; i < 32; i++)
            {
                VkMemoryPropertyFlags flag = 1 << i;

                if (flags & flag)
                {
                    log_debug(root, "   - {}", vk_to_string(flag));
                    flags ^= flag;
                }
            }
        }
    }

    void vulkan_choose_physical_device(ReaperRoot& root, VulkanBackend& backend,
                                       std::span<const char*> device_extensions)
    {
        uint32_t deviceCount = 0;

        AssertVk(vkEnumeratePhysicalDevices(backend.instance, &deviceCount, nullptr));
        Assert(deviceCount > 0);

        log_debug(root, "vulkan: enumerating {} physical devices", deviceCount);

        std::vector<VkPhysicalDevice> available_physical_devices(deviceCount);
        AssertVk(vkEnumeratePhysicalDevices(backend.instance, &deviceCount, available_physical_devices.data()));

        bool found_physical_device = false;

        for (auto& physical_device_handle : available_physical_devices)
        {
            backend.physical_device =
                create_physical_device_info(physical_device_handle, root.renderer->window, backend.presentInfo.surface);

            if (vulkan_check_physical_device(backend.physical_device, physical_device_handle, device_extensions))
            {
                found_physical_device = true;
                break;
            }
        }

        Assert(found_physical_device, "could not select physical device based on the chosen properties");

        vulkan_print_physical_device_debug(root, backend.physical_device);
    }

    VkDescriptorPool create_global_descriptor_pool(ReaperRoot& root, VulkanBackend& backend)
    {
        // Create descriptor pool
        // FIXME Sizes are arbitrary for now, as long as everything fits
        constexpr u32                     MaxDescriptorSets = 100;
        std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 16},        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32},  {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16}};

        const VkDescriptorPoolCreateInfo poolInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = VK_FLAGS_NONE,
                                                     .maxSets = MaxDescriptorSets,
                                                     .poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()),
                                                     .pPoolSizes = descriptorPoolSizes.data()};

        VkDescriptorPool pool = VK_NULL_HANDLE;
        AssertVk(vkCreateDescriptorPool(backend.device, &poolInfo, nullptr, &pool));
        log_debug(root, "vulkan: created descriptor pool with handle: {}", static_cast<void*>(pool));

        return pool;
    }

} // namespace

void create_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE_FUNC();
    log_info(root, "vulkan: creating backend");

    log_debug(root, "vulkan: loading {}", REAPER_VK_LIB_NAME);
    backend.vulkanLib = dynlib::load(REAPER_VK_LIB_NAME);

    vulkan_load_exported_functions(backend.vulkanLib);
    vulkan_load_global_level_functions();

    std::vector<const char*> instance_extensions = {
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        REAPER_VK_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
    };

#if REAPER_DEBUG
    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    log_info(root, "vulkan: using {} instance level extensions", instance_extensions.size());
    for (auto& e : instance_extensions)
        log_debug(root, "- {}", e);

    vulkan_instance_check_extensions(instance_extensions);

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

    const VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "MyGame",
        .applicationVersion = appVersion,
        .pEngineName = "Reaper",
        .engineVersion = engineVersion,
        .apiVersion = vulkanVersion,
    };

    const VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = static_cast<u32>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<u32>(instance_extensions.size()),
        .ppEnabledExtensionNames = instance_extensions.data(),
    };

    AssertVk(vkCreateInstance(&instance_create_info, nullptr, &backend.instance));

    vulkan_load_instance_level_functions(backend.instance);

#if REAPER_DEBUG
    log_debug(root, "vulkan: attaching debug callback");
    backend.debugMessenger = vulkan_create_debug_callback(backend.instance, root);
#endif

    WindowCreationDescriptor windowDescriptor = {
        .title = "Vulkan",
        .width = 2560,
        .height = 1440,
        .fullscreen = false,
    };

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
        VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
        VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME,
        VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME,
    };

    log_debug(root, "vulkan: choosing physical device");
    vulkan_choose_physical_device(root, backend, device_extensions);

    log_debug(root, "vulkan: creating logical device");
    vulkan_create_logical_device(root, backend, device_extensions);

    log_debug(root, "vulkan: create global descriptor pool");
    backend.global_descriptor_pool = create_global_descriptor_pool(root, backend);

    log_debug(root, "vulkan: create gpu memory allocator");

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = backend.physical_device.handle;
    allocatorInfo.device = backend.device;
    allocatorInfo.instance = backend.instance;

    vmaCreateAllocator(&allocatorInfo, &backend.vma_instance);

    SwapchainDescriptor swapchainDesc;
    swapchainDesc.preferredImageCount = 3;
    swapchainDesc.preferredFormat = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_MAX_ENUM_KHR};
    swapchainDesc.preferredExtent = {windowDescriptor.width, windowDescriptor.height};

    configure_vulkan_wm_swapchain(root, backend, swapchainDesc, backend.presentInfo);
    create_vulkan_wm_swapchain(root, backend, backend.presentInfo);

    set_backend_render_resolution(backend);

    // create_vulkan_display_swapchain(root, backend);

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
    };

    backend.semaphore_swapchain_image_available =
        create_semaphore(backend, semaphore_create_info, "Semaphore image available");

    ImGui::CreateContext();

    ImGui_ImplVulkan_InitInfo imgui_vulkan_init_info = {
        .Instance = backend.instance,
        .PhysicalDevice = backend.physical_device.handle,
        .Device = backend.device,
        .QueueFamily = 0, // FIXME
        .Queue = backend.graphics_queue,
        .PipelineCache = nullptr,
        .DescriptorPool = backend.global_descriptor_pool,
        .Subpass = 0,
        .MinImageCount = 3,                   // FIXME
        .ImageCount = 3,                      // FIXME
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT, // FIXME
        .Allocator = nullptr,
        .CheckVkResultFn = nullptr,
    };

    ImGui_ImplVulkan_LoadFunctions(nullptr, nullptr);
    ImGui_ImplVulkan_Init(&imgui_vulkan_init_info, PixelFormatToVulkan(GUIFormat));

    log_info(root, "vulkan: ready");
}

void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE_FUNC();
    log_info(root, "vulkan: destroying backend");

    log_debug(root, "vulkan: waiting for current work to finish");
    AssertVk(vkDeviceWaitIdle(backend.device));

    ImGui_ImplVulkan_Shutdown();

    vkDestroySemaphore(backend.device, backend.semaphore_swapchain_image_available, nullptr);

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
    vulkan_destroy_debug_callback(backend.instance, backend.debugMessenger);
#endif

    vkDestroyInstance(backend.instance, nullptr);

    log_debug(root, "vulkan: unloading {}", REAPER_VK_LIB_NAME);
    Assert(backend.vulkanLib != nullptr);
    dynlib::close(backend.vulkanLib);
    backend.vulkanLib = nullptr;
}

void set_backend_render_resolution(VulkanBackend& backend)
{
    // NOTE: We can change the render resolution scaling here
    backend.render_extent = {
        .width = backend.presentInfo.surface_extent.width,
        .height = backend.presentInfo.surface_extent.height,
    };
}
} // namespace Reaper
