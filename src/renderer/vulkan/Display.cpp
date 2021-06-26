////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Display.h"

#include "Backend.h"
#include "api/Vulkan.h"

#include "common/Log.h"

#include "core/Profile.h"

namespace Reaper
{
#ifdef REAPER_VK_USE_DISPLAY_EXTENSIONS
void create_vulkan_display_swapchain(ReaperRoot& root, const VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE("Vulkan", MP_RED);
    log_debug(root, "vulkan: creating display swapchain");

    uint32_t displayCount = 0;

    Assert(vkGetPhysicalDeviceDisplayPropertiesKHR(backend.physicalDevice, &displayCount, nullptr) == VK_SUCCESS);
    Assert(displayCount > 0);

    log_debug(root, "vulkan: enumerating {} displays", displayCount);

    std::vector<VkDisplayPropertiesKHR> availableDisplayProperties(displayCount);
    Assert(vkGetPhysicalDeviceDisplayPropertiesKHR(backend.physicalDevice, &displayCount,
                                                   availableDisplayProperties.data())
           == VK_SUCCESS);

    for (const VkDisplayPropertiesKHR& properties : availableDisplayProperties)
    {
        log_debug(root, "- name = '{}', dimensions = {}x{}mm, native res = {}x{}", properties.displayName,
                  properties.physicalDimensions.width, properties.physicalDimensions.height,
                  properties.physicalResolution.width, properties.physicalResolution.height);
        log_debug(root, "plane reordering support = {}, persistent content = {}", properties.planeReorderPossible,
                  properties.persistentContent);
    }

    uint32_t planeCount = 0;

    Assert(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(backend.physicalDevice, &planeCount, nullptr) == VK_SUCCESS);
    Assert(planeCount > 0);

    log_debug(root, "vulkan: enumerating {} display planes", planeCount);

    std::vector<VkDisplayPlanePropertiesKHR> displayPlaneProperties(planeCount);
    Assert(
        vkGetPhysicalDeviceDisplayPlanePropertiesKHR(backend.physicalDevice, &planeCount, displayPlaneProperties.data())
        == VK_SUCCESS);

    for (size_t i = 0; i < planeCount; i++)
    {
        const VkDisplayPlanePropertiesKHR& properties = displayPlaneProperties[i];
        // TODO
        static_cast<void>(properties);
    }
}

void destroy_vulkan_display_swapchain(ReaperRoot& root, const VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE("Vulkan", MP_RED);
    log_debug(root, "vulkan: destroying display swapchain");

    // TODO
    static_cast<void>(backend);
}
#endif
} // namespace Reaper
