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
    VkPhysicalDeviceProperties                    properties;
    VkPhysicalDeviceDriverProperties              driver_properties;
    VkPhysicalDeviceSubgroupProperties            subgroup_properties;
    VkPhysicalDeviceSubgroupSizeControlProperties subgroup_size_control_properties;
    VkPhysicalDeviceMemoryProperties              memory_properties;

    // NOTE: Filled with fill_physical_device_supported_features()
    VkPhysicalDeviceFeatures                                features;
    VkPhysicalDeviceVulkan12Features                        vulkan1_2_features;
    VkPhysicalDeviceVulkan13Features                        vulkan1_3_features;
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
