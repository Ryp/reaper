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
#include "Display.h"
#include "Swapchain.h"

#include <cstring>
#include <iostream>

#include "PresentationSurface.h"
#include "api/VulkanStringConversion.h"
#include "renderer/window/Window.h"

#include "BackendResources.h"

#include <backends/imgui_impl_vulkan.h>

#ifndef REAPER_VK_LIB_NAME
#    error
#endif

// Version of the API to query when loading vulkan symbols
#define REAPER_VK_API_VERSION VK_MAKE_VERSION(1, 3, 0)

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
    VkSemaphore create_semaphore(VulkanBackend& backend, const char* debug_name,
                                 const VkSemaphoreCreateInfo& create_info)
    {
        VkSemaphore semaphore;
        Assert(vkCreateSemaphore(backend.device, &create_info, nullptr, &semaphore) == VK_SUCCESS);

        VulkanSetDebugName(backend.device, semaphore, debug_name);

        return semaphore;
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

    Assert(vkCreateInstance(&instance_create_info, nullptr, &backend.instance) == VK_SUCCESS,
           "cannot create Vulkan instance");

    vulkan_load_instance_level_functions(backend.instance);

#if REAPER_DEBUG
    log_debug(root, "vulkan: attaching debug callback");
    vulkan_setup_debug_callback(root, backend);
#endif

    WindowCreationDescriptor windowDescriptor;
    windowDescriptor.title = "Vulkan";
    windowDescriptor.width = 2560;
    windowDescriptor.height = 1440;
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
#if REAPER_WINDOWS_HDR_TEST
        VK_EXT_HDR_METADATA_EXTENSION_NAME,
        VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,
        VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME,
#endif
        VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
        VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME,
        VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME,
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
    swapchainDesc.preferredImageCount = 3;
    swapchainDesc.preferredFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    swapchainDesc.preferredFormat = {VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT};

    swapchainDesc.preferredExtent = {windowDescriptor.width, windowDescriptor.height};

    configure_vulkan_wm_swapchain(root, backend, swapchainDesc, backend.presentInfo);
    create_vulkan_wm_swapchain(root, backend, backend.presentInfo);

    // create_vulkan_display_swapchain(root, backend);

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
    };

    backend.semaphore_swapchain_image_available =
        create_semaphore(backend, "Semaphore image available", semaphore_create_info);
    backend.semaphore_rendering_finished =
        create_semaphore(backend, "Semaphore rendering finished", semaphore_create_info);

    {
        // this initializes the core structures of imgui
        ImGui::CreateContext();

        // this initializes imgui for SDL
        // ImGui_ImplSDL2_InitForVulkan(_window);

        // this initializes imgui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = backend.instance;
        init_info.PhysicalDevice = backend.physicalDevice;
        init_info.Device = backend.device;
        init_info.Queue = backend.deviceInfo.graphicsQueue;
        init_info.DescriptorPool = backend.global_descriptor_pool;
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        // constexpr PixelFormat GUIFormat = PixelFormat::R8G8B8A8_SRGB;// FIXME
        // const VkFormat gui_format = PixelFormatToVulkan(GUIFormat);
        ImGui_ImplVulkan_LoadFunctions(nullptr, nullptr);
        ImGui_ImplVulkan_Init(&init_info, VK_FORMAT_R8G8B8A8_SRGB);
    }

    log_info(root, "vulkan: ready");
}

void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE_FUNC();
    log_info(root, "vulkan: destroying backend");

    log_debug(root, "vulkan: waiting for current work to finish");
    Assert(vkDeviceWaitIdle(backend.device) == VK_SUCCESS);

    ImGui_ImplVulkan_Shutdown();

    vkDestroySemaphore(backend.device, backend.semaphore_swapchain_image_available, nullptr);
    vkDestroySemaphore(backend.device, backend.semaphore_rendering_finished, nullptr);

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
    void print_debug_message(ReaperRoot* root, const VkDebugUtilsMessengerCallbackDataEXT* callback_data)
    {
        const char* id_name = callback_data->pMessageIdName;
        log_info(*root, "vulkan: debug message [id: {} ({:#010x})] {}", id_name ? id_name : "unnamed",
                 callback_data->messageIdNumber, callback_data->pMessage);

        for (auto& queue_label : std::span(callback_data->pQueueLabels, callback_data->queueLabelCount))
        {
            const char* label = queue_label.pLabelName;
            log_info(*root, "- queue label: '{}'", label ? label : "unnamed");
        }

        for (auto& command_buffer_label : std::span(callback_data->pCmdBufLabels, callback_data->cmdBufLabelCount))
        {
            const char* label = command_buffer_label.pLabelName;
            log_info(*root, "- command buffer label: '{}'", label ? label : "unnamed");
        }

        for (auto& object_name_info : std::span(callback_data->pObjects, callback_data->objectCount))
        {
            const char* label = object_name_info.pObjectName;
            log_info(*root, "- object '{}', type = {}, handle = {:#018x}", label ? label : "unnamed",
                     vk_to_string(object_name_info.objectType), object_name_info.objectHandle);
        }
    }

    VkBool32 VKAPI_PTR debug_message_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                              VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
                                              const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                              void*                                       user_data)
    {
        ReaperRoot* root = static_cast<ReaperRoot*>(user_data);
        Assert(root != nullptr);

        constexpr i32 ID_LoaderMessage = 0x0000000;
        constexpr i32 ID_UNASSIGNED_BestPractices_vkCreateDevice_specialuse_extension_glemulation =
            -0x703c3ecb; // CreateDevice(): Attempting to enable extension VK_EXT_primitive_topology_list_restart, but
                         // this extension is intended to support OpenGL and/or OpenGL ES emulation layers, and
                         // applications ported from those APIs, by adding functionality specific to those APIs and it
                         // is strongly recommended that it be otherwise avoided.
        constexpr i32 ID_UNASSIGNED_BestPractices_vkAllocateMemory_small_allocation =
            -0x23e75295; // vkAllocateMemory(): Allocating a VkDeviceMemory of size 131072. This is a very small
                         // allocation (current threshold is 262144 bytes). You should make large allocations and
                         // sub-allocate from one large VkDeviceMemory.
        constexpr i32 ID_UNASSIGNED_BestPractices_vkBindMemory_small_dedicated_allocation =
            -0x4c2bcb95; // vkBindImageMemory(): Trying to bind VkImage 0xb9b24e0000000113[] to a memory block which is
                         // fully consumed by the image. The required size of the allocation is 131072, but smaller
                         // images like this should be sub-allocated from larger memory blocks. (Current threshold is
                         // 1048576 bytes.)
        constexpr i32 ID_UNASSIGNED_BestPractices_pipeline_stage_flags =
            0x48a09f6c; // You are using VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR when vkCmdResetEvent2 is called
        constexpr i32 ID_UNASSIGNED_BestPractices_CreatePipelines_AvoidPrimitiveRestart =
            0x4d6711e7; // [AMD] Performance warning: Use of primitive restart is not recommended
        constexpr i32 ID_UNASSIGNED_BestPractices_vkImage_DontUseStorageRenderTargets =
            -0x33200141; // [AMD] Performance warning: image 'Lighting' is created as a render target with
                         // VK_IMAGE_USAGE_STORAGE_BIT. Using a VK_IMAGE_USAGE_STORAGE_BIT is not recommended with color
                         // and depth targets
        constexpr i32 ID_UNASSIGNED_BestPractices_CreateDevice_PageableDeviceLocalMemory =
            0x2e99adca; // [NVIDIA] vkCreateDevice() called without pageable device local memory. Use
                        // pageableDeviceLocalMemory from VK_EXT_pageable_device_local_memory when it is available.
        constexpr i32 ID_UNASSIGNED_BestPractices_AllocateMemory_SetPriority =
            0x61f61757; // [NVIDIA] Use VkMemoryPriorityAllocateInfoEXT to provide the operating system information on
                        // the allocations that should stay in video memory and which should be demoted first when video
                        // memory is limited. The highest priority should be given to GPU-written resources like color
                        // attachments, depth attachments, storage images, and buffers written from the GPU.
        constexpr i32 ID_UNASSIGNED_BestPractices_CreatePipelineLayout_SeparateSampler =
            0x362cd642; // [NVIDIA] Consider using combined image samplers instead of separate samplers for marginally
                        // better performance.
        constexpr i32 ID_UNASSIGNED_BestPractices_vkBeginCommandBuffer_one_time_submit =
            -0x461324b6; // [NVIDIA] vkBeginCommandBuffer(): VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT was not set and
                         // the command buffer has only been submitted once. For best performance on NVIDIA GPUs, use
                         // ONE_TIME_SUBMIT.
        constexpr i32 ID_UNASSIGNED_BestPractices_Zcull_LessGreaterRatio =
            -0xa56a353; // [NVIDIA] Depth attachment VkImage 0xd22318000000014b[Tile Depth Max] is primarily rendered
                        // with depth compare op LESS, but some draws use GREATER. Z-cull is disabled for the least used
                        // direction, which harms depth testing performance. The Z-cull direction can be reset by
                        // clearing the depth attachment, transitioning from VK_IMAGE_LAYOUT_UNDEFINED, using
                        // VK_ATTACHMENT_LOAD_OP_DONT_CARE, or using VK_ATTACHMENT_STORE_OP_DONT_CARE.
        constexpr i32 ID_UNASSIGNED_BestPractices_AllocateMemory_ReuseAllocations =
            0x6e57f7a6; // [NVIDIA] Reuse memory allocations instead of releasing and reallocating. A memory allocation
                        // has just been released, and it could have been reused in place of this allocation.

        std::vector<i32> ignored_ids = {
            ID_LoaderMessage,
            ID_UNASSIGNED_BestPractices_vkCreateDevice_specialuse_extension_glemulation,
            ID_UNASSIGNED_BestPractices_vkAllocateMemory_small_allocation,
            ID_UNASSIGNED_BestPractices_vkBindMemory_small_dedicated_allocation,
            ID_UNASSIGNED_BestPractices_pipeline_stage_flags,
            ID_UNASSIGNED_BestPractices_CreatePipelines_AvoidPrimitiveRestart,
            ID_UNASSIGNED_BestPractices_vkImage_DontUseStorageRenderTargets,
            ID_UNASSIGNED_BestPractices_CreateDevice_PageableDeviceLocalMemory,
            ID_UNASSIGNED_BestPractices_AllocateMemory_SetPriority,
            ID_UNASSIGNED_BestPractices_CreatePipelineLayout_SeparateSampler,
            ID_UNASSIGNED_BestPractices_vkBeginCommandBuffer_one_time_submit,
            ID_UNASSIGNED_BestPractices_Zcull_LessGreaterRatio,
            ID_UNASSIGNED_BestPractices_AllocateMemory_ReuseAllocations,
        };

        bool ignore_message = false;
        for (auto ignored_id : ignored_ids)
        {
            if (ignored_id == callback_data->messageIdNumber)
                ignore_message = true;
        }

        if (!ignore_message)
        {
            print_debug_message(root, callback_data);
        }

        const bool ignore_assert = ignore_message || severity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        Assert(ignore_assert, callback_data->pMessage);

        bool exit_on_assert = false;
        if (!ignore_assert && exit_on_assert)
        {
            exit(EXIT_FAILURE);
        }

        return VK_FALSE;
    }
} // namespace

void vulkan_setup_debug_callback(ReaperRoot& root, VulkanBackend& backend)
{
    VkDebugUtilsMessengerCreateInfoEXT callbackCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &debug_message_callback,
        .pUserData = &root,
    };

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

    VkPhysicalDeviceIndexTypeUint8FeaturesEXT index_uint8_feature = {};
    index_uint8_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;

    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT primitive_restart_feature = {};
    primitive_restart_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;
    primitive_restart_feature.pNext = &index_uint8_feature;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features = {};
    descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptor_indexing_features.pNext = &primitive_restart_feature;

    VkPhysicalDeviceVulkan13Features device_vulkan13_features = {};
    device_vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    device_vulkan13_features.pNext = &descriptor_indexing_features;

    VkPhysicalDeviceVulkan12Features device_vulkan12_features = {};
    device_vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    device_vulkan12_features.pNext = &device_vulkan13_features;

    VkPhysicalDeviceFeatures2 device_features2 = {};
    device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features2.pNext = &device_vulkan12_features;

    vkGetPhysicalDeviceFeatures2(physical_device, &device_features2);

    Assert(device_properties.apiVersion >= REAPER_VK_API_VERSION,
           "Unsupported Vulkan version. Is your GPU driver too old?");
    Assert(device_properties.limits.maxImageDimension2D >= 4096);
    Assert(device_features2.features.samplerAnisotropy == VK_TRUE);
    Assert(device_features2.features.multiDrawIndirect == VK_TRUE);
    Assert(device_features2.features.drawIndirectFirstInstance == VK_TRUE);
    Assert(device_features2.features.fragmentStoresAndAtomics == VK_TRUE);
    Assert(device_features2.features.fillModeNonSolid == VK_TRUE);
    Assert(device_features2.features.geometryShader == VK_TRUE);
    Assert(device_vulkan12_features.shaderSampledImageArrayNonUniformIndexing == VK_TRUE);
    Assert(device_vulkan13_features.synchronization2 == VK_TRUE);
    Assert(device_vulkan13_features.dynamicRendering == VK_TRUE);
    Assert(device_vulkan13_features.computeFullSubgroups == VK_TRUE);
    Assert(primitive_restart_feature.primitiveTopologyListRestart == VK_TRUE);
    Assert(index_uint8_feature.indexTypeUint8 == VK_TRUE);

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

    physicalDeviceInfo.subgroup_size = subgroupProperties.subgroupSize;

    Assert(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT);
    Assert(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
    Assert(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT);
    Assert(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT);

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

    log_debug(root, "- subgroup size = {}", physicalDeviceInfo.subgroup_size);

    std::span<const VkMemoryHeap> memory_heaps(physicalDeviceInfo.memory.memoryHeaps,
                                               physicalDeviceInfo.memory.memoryHeapCount);

    for (u32 heap_index = 0; heap_index < memory_heaps.size(); heap_index++)
    {
        const VkMemoryHeap& heap = memory_heaps[heap_index];
        log_debug(root, "- heap_index {}: available size = {}, flags = {}", heap_index, heap.size, heap.flags);
    }

    std::span<const VkMemoryType> memory_types(physicalDeviceInfo.memory.memoryTypes,
                                               physicalDeviceInfo.memory.memoryTypeCount);

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

    backend.physicalDevice = chosenPhysicalDevice;
    backend.physicalDeviceProperties = physicalDeviceProperties;
}

void vulkan_create_logical_device(ReaperRoot&                     root,
                                  VulkanBackend&                  backend,
                                  const std::vector<const char*>& device_extensions)
{
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::vector<float>                   queue_priorities = {1.0f};

    queue_create_infos.push_back(VkDeviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .queueFamilyIndex = backend.physicalDeviceInfo.graphicsQueueFamilyIndex,
        .queueCount = static_cast<uint32_t>(queue_priorities.size()),
        .pQueuePriorities = queue_priorities.data(),
    });

    if (backend.physicalDeviceInfo.graphicsQueueFamilyIndex != backend.physicalDeviceInfo.presentQueueFamilyIndex)
    {
        queue_create_infos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FLAGS_NONE,
            .queueFamilyIndex = backend.physicalDeviceInfo.presentQueueFamilyIndex,
            .queueCount = static_cast<uint32_t>(queue_priorities.size()),
            .pQueuePriorities = queue_priorities.data(),
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
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.drawIndirectFirstInstance = VK_TRUE;
    deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.geometryShader = VK_TRUE;

    VkPhysicalDeviceIndexTypeUint8FeaturesEXT index_uint8_feature = {};
    index_uint8_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
    index_uint8_feature.indexTypeUint8 = VK_TRUE;

    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT primitive_restart_feature = {};
    primitive_restart_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;
    primitive_restart_feature.pNext = &index_uint8_feature;
    primitive_restart_feature.primitiveTopologyListRestart = VK_TRUE;

    VkPhysicalDeviceVulkan13Features device_vulkan13_features = {};
    device_vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    device_vulkan13_features.pNext = &primitive_restart_feature;
    device_vulkan13_features.synchronization2 = VK_TRUE;
    device_vulkan13_features.dynamicRendering = VK_TRUE;
    device_vulkan13_features.computeFullSubgroups = VK_TRUE;

    VkPhysicalDeviceVulkan12Features device_features_1_2 = {};
    device_features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    device_features_1_2.pNext = &device_vulkan13_features;
    device_features_1_2.drawIndirectCount = VK_TRUE;
    device_features_1_2.imagelessFramebuffer = VK_TRUE;
    device_features_1_2.separateDepthStencilLayouts = VK_TRUE;
    device_features_1_2.descriptorIndexing = VK_TRUE;
    device_features_1_2.runtimeDescriptorArray = VK_TRUE;
    device_features_1_2.descriptorBindingPartiallyBound = VK_TRUE;
    device_features_1_2.timelineSemaphore = VK_TRUE;
    device_features_1_2.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &device_features_1_2;
    deviceFeatures2.features = deviceFeatures;

    const VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures2,
        .flags = VK_FLAGS_NONE,
        .queueCreateInfoCount = queueCreateCount,
        .pQueueCreateInfos = &queue_create_infos[0],
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<u32>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
        .pEnabledFeatures = nullptr,
    };

    Assert(vkCreateDevice(backend.physicalDevice, &device_create_info, nullptr, &backend.device) == VK_SUCCESS,
           "could not create Vulkan device");

    VulkanSetDebugName(backend.device, backend.device, "Device");

    vulkan_load_device_level_functions(backend.device);

    vkGetDeviceQueue(backend.device, backend.physicalDeviceInfo.graphicsQueueFamilyIndex, 0,
                     &backend.deviceInfo.graphicsQueue);
    vkGetDeviceQueue(backend.device, backend.physicalDeviceInfo.presentQueueFamilyIndex, 0,
                     &backend.deviceInfo.presentQueue);
}
} // namespace Reaper
