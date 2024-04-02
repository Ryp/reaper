////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PhysicalDevice.h"
#include "PresentationSurface.h"

#include "api/AssertHelper.h"
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

        physical_device.properties_vk_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
        physical_device.properties_vk_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
        physical_device.properties_vk_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;

        vk_hook(physical_device_properties_2,
                vk_hook(physical_device.properties_vk_1_1,
                        vk_hook(physical_device.properties_vk_1_2, vk_hook(physical_device.properties_vk_1_3))));

        vkGetPhysicalDeviceProperties2(handle, &physical_device_properties_2);

        physical_device.properties = physical_device_properties_2.properties;

        vkGetPhysicalDeviceMemoryProperties(handle, &physical_device.memory_properties);
    }

    void fill_physical_device_supported_features(PhysicalDeviceInfo& physical_device, VkPhysicalDevice handle)
    {
        // NOTE: We only keep data for the VkPhysicalDeviceFeatures struct so this one is local
        VkPhysicalDeviceFeatures2 physical_device_features_2;
        physical_device_features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        physical_device.features_vk_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        physical_device.features_vk_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        physical_device.shader_atomic_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
        physical_device.index_uint8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
        physical_device.primitive_restart_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;

        vk_hook(physical_device_features_2,
                vk_hook(physical_device.features_vk_1_2,
                        vk_hook(physical_device.features_vk_1_3,
                                vk_hook(physical_device.shader_atomic_features,
                                        vk_hook(physical_device.index_uint8_features,
                                                vk_hook(physical_device.primitive_restart_features))))));

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
            AssertVk(vkGetPhysicalDeviceSurfaceSupportKHR(handle, i, presentationSurface, &queue_present_support[i]));

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

    bool check_support_for_compute_writable_depth_format(VkPhysicalDevice physical_device)
    {
        // FIXME This way of detection is a bit brittle.
        // We're not exactly matching what the render pass code will do
        // NOTE: Maybe we could have detection code per-renderpass instead.
        const VkPhysicalDeviceImageFormatInfo2 compute_writable_depth_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
            .pNext = nullptr,
            .format = VK_FORMAT_D16_UNORM, // Only checking one format here
            .type = VK_IMAGE_TYPE_2D,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .flags = VK_FLAGS_NONE,
        };

        VkImageFormatProperties2 image_properties;
        image_properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
        image_properties.pNext = nullptr;

        const VkResult result =
            vkGetPhysicalDeviceImageFormatProperties2(physical_device, &compute_writable_depth_info, &image_properties);

        switch (result)
        {
        case VK_SUCCESS:
            return true;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return false;
        default:
            AssertVk(result);
            return false;
        }
    }

    PhysicalDeviceInfo::MacroFeatures fill_physical_device_info_features(VkPhysicalDevice handle)
    {
        return PhysicalDeviceInfo::MacroFeatures{
            .compute_stores_to_depth = check_support_for_compute_writable_depth_format(handle),
        };
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

    physical_device.macro_features = fill_physical_device_info_features(handle);

    return physical_device;
}
} // namespace Reaper
