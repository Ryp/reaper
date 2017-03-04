////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Vulkan.h"

namespace vk {

#define REAPER_VK_EXPORTED_FUNCTION(func)           PFN_##func func;
#define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func)       PFN_##func func;
#define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func)     PFN_##func func;
#define REAPER_VK_DEVICE_LEVEL_FUNCTION(func)       PFN_##func func;
#include "VulkanSymbolHelper.inl"

void vulkan_load_exported_functions(LibHandle vulkanLib)
{
    #define REAPER_VK_EXPORTED_FUNCTION(func) {                                 \
        func = (PFN_##func)dynlib::getSymbol(vulkanLib, #func); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"
}

void vulkan_load_global_level_functions()
{
    #define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func) {                             \
        func = (PFN_##func)vkGetInstanceProcAddr(nullptr, #func);               \
        Assert(func != nullptr, "could not load global level vk function"); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"
}

void vulkan_load_instance_level_functions(VkInstance instance)
{
    #define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func) {                           \
        func = (PFN_##func)vkGetInstanceProcAddr(instance, #func);     \
        Assert(func != nullptr, "could not load instance level vk function"); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"
}

void vulkan_load_device_level_functions(VkDevice device)
{
    #define REAPER_VK_DEVICE_LEVEL_FUNCTION(func) {                             \
        func = (PFN_##func)vkGetDeviceProcAddr(device, #func);                  \
        Assert(func != nullptr, "could not load device level vk function"); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"
}

}
