////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "vulkan_loader/Vulkan.h"

namespace Reaper
{
struct PhysicalDeviceInfo
{
    VkPhysicalDevice handle;

    // NOTE: Filled with fill_physical_device_properties()
    VkPhysicalDeviceProperties         properties;
    VkPhysicalDeviceVulkan11Properties properties_vk_1_1;
    VkPhysicalDeviceVulkan12Properties properties_vk_1_2;
    VkPhysicalDeviceVulkan13Properties properties_vk_1_3;
    VkPhysicalDeviceMemoryProperties   memory_properties;

    // NOTE: Filled with fill_physical_device_supported_features()
    VkPhysicalDeviceFeatures                                features;
    VkPhysicalDeviceVulkan12Features                        features_vk_1_2;
    VkPhysicalDeviceVulkan13Features                        features_vk_1_3;
    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT            shader_atomic_features;
    VkPhysicalDeviceIndexTypeUint8FeaturesEXT               index_uint8_features;
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT primitive_restart_features;

    // NOTE: Filled with fill_physical_device_supported_queues()
    // These can point to the same object!
    uint32_t graphics_queue_family_index;
    uint32_t present_queue_family_index;

    struct MacroFeatures
    {
        bool compute_stores_to_depth = false;
    } macro_features;
};

class IWindow;

PhysicalDeviceInfo create_physical_device_info(VkPhysicalDevice handle, IWindow* window,
                                               VkSurfaceKHR presentationSurface);
} // namespace Reaper
