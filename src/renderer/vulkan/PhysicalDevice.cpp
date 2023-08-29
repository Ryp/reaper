////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PhysicalDevice.h"
#include "PresentationSurface.h"

#include "api/VulkanHook.h"

#include "core/Assert.h"

#include <vector>

namespace Reaper
{
namespace
{
    void fill_physical_device_properties(PhysicalDeviceInfo& physical_device, VkPhysicalDevice handle)
    {
        // NOTE: We only keep data for the VkPhysicalDeviceProperties struct so this one is local
        VkPhysicalDeviceProperties2 physical_device_properties_2;
        physical_device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        physical_device.driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        physical_device.subgroup_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

        vk_hook(physical_device_properties_2,
                vk_hook(physical_device.subgroup_properties, vk_hook(physical_device.driver_properties)));

        vkGetPhysicalDeviceProperties2(handle, &physical_device_properties_2);

        physical_device.properties = physical_device_properties_2.properties;

        vkGetPhysicalDeviceMemoryProperties(handle, &physical_device.memory_properties);
    }

    void fill_physical_device_supported_features(PhysicalDeviceInfo& physical_device, VkPhysicalDevice handle)
    {
        // NOTE: We only keep data for the VkPhysicalDeviceFeatures struct so this one is local
        VkPhysicalDeviceFeatures2 physical_device_features_2;
        physical_device_features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        physical_device.vulkan1_2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        physical_device.vulkan1_3_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        physical_device.index_uint8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
        physical_device.primitive_restart_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;

        vk_hook(physical_device_features_2,
                vk_hook(physical_device.vulkan1_2_features,
                        vk_hook(physical_device.vulkan1_3_features,
                                vk_hook(physical_device.index_uint8_features,
                                        vk_hook(physical_device.primitive_restart_features)))));

        vkGetPhysicalDeviceFeatures2(handle, &physical_device_features_2);

        physical_device.features = physical_device_features_2.features;
    }

    void fill_physical_device_supported_queues(PhysicalDeviceInfo& physical_device, VkPhysicalDevice handle,
                                               IWindow* window, VkSurfaceKHR presentationSurface)
    {
        Assert(window != nullptr);

        physical_device.graphics_queue_family_index = UINT32_MAX;
        physical_device.present_queue_family_index = UINT32_MAX;

        uint32_t queue_families_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(handle, &queue_families_count, nullptr);

        Assert(queue_families_count > 0, "device doesn't have any queue families");
        if (queue_families_count == 0)
        {
            return;
        }

        std::vector<VkQueueFamilyProperties> queue_family_properties(queue_families_count);
        std::vector<VkBool32>                queue_present_support(queue_families_count);

        vkGetPhysicalDeviceQueueFamilyProperties(handle, &queue_families_count, &queue_family_properties[0]);

        for (uint32_t i = 0; i < queue_families_count; ++i)
        {
            Assert(vkGetPhysicalDeviceSurfaceSupportKHR(handle, i, presentationSurface, &queue_present_support[i])
                   == VK_SUCCESS);

            if ((queue_family_properties[i].queueCount > 0)
                && (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                // Select first queue that supports graphics
                if (physical_device.graphics_queue_family_index == UINT32_MAX)
                {
                    physical_device.graphics_queue_family_index = i;
                }

                Assert(vulkan_queue_family_has_presentation_support(handle, i, window)
                           == (queue_present_support[i] == VK_TRUE),
                       "Queue family presentation support mismatch.");

                // If there is queue that supports both graphics and present - prefer it
                if (queue_present_support[i])
                {
                    physical_device.graphics_queue_family_index = i;
                    physical_device.present_queue_family_index = i;
                    return;
                }
            }
        }

        // We don't have queue that supports both graphics and present so we have to use separate queues
        for (uint32_t i = 0; i < queue_families_count; ++i)
        {
            if (queue_present_support[i] == VK_TRUE)
            {
                physical_device.present_queue_family_index = i;
                break;
            }
        }
    }
} // namespace

PhysicalDeviceInfo create_physical_device_info(VkPhysicalDevice handle, IWindow* window,
                                               VkSurfaceKHR presentationSurface)
{
    PhysicalDeviceInfo physical_device;

    physical_device.handle = handle;

    fill_physical_device_properties(physical_device, handle);
    fill_physical_device_supported_features(physical_device, handle);
    fill_physical_device_supported_queues(physical_device, handle, window, presentationSurface);

    return physical_device;
}
} // namespace Reaper
