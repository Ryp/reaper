////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VulkanRenderer.h"

#include "core/DynamicLibrary.h"
#include "api/Vulkan.h"
using namespace vk;

void VulkanRenderer::run()
{
    LibHandle vulkanLib = dynlib::load(REAPER_VK_LIB_NAME);

    #define REAPER_VK_EXPORTED_FUNCTION(func) {                                 \
        func = (PFN_##func)dynlib::getSymbol(vulkanLib, #func); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    #define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func) {                             \
        func = (PFN_##func)vkGetInstanceProcAddr(nullptr, #func);               \
        Assert(func != nullptr, "could not load global vk function"); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    VkInstance vkInstance(VK_NULL_HANDLE);

    VkApplicationInfo application_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType            sType
        nullptr,                            // const void                *pNext
        "Tower Defense FTW",                // const char                *pApplicationName
        VK_MAKE_VERSION(1, 0, 0),           // uint32_t                   applicationVersion
        "Reaper",                           // const char                *pEngineName
        VK_MAKE_VERSION(1, 0, 0),           // uint32_t                   engineVersion
        VK_MAKE_VERSION(1, 0, 8)            // uint32_t                   apiVersion
    };

    VkInstanceCreateInfo instance_create_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType            sType
        nullptr,                                // const void*                pNext
        0,                                      // VkInstanceCreateFlags      flags
        &application_info,                      // const VkApplicationInfo   *pApplicationInfo
        0,                                      // uint32_t                   enabledLayerCount
        nullptr,                                // const char * const        *ppEnabledLayerNames
        0,                                      // uint32_t                   enabledExtensionCount
        nullptr                                 // const char * const        *ppEnabledExtensionNames
    };

    Assert(vkCreateInstance(&instance_create_info, nullptr, &vkInstance) == VK_SUCCESS, "can not create Vulkan instance");

    #define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func) {                           \
        func = (PFN_##func)vkGetInstanceProcAddr(vkInstance, #func);            \
        Assert(func != nullptr); }
    #include "renderer/vulkan/api/VulkanSymbolHelper.inl"

    vkDestroyInstance(vkInstance, nullptr);
    dynlib::close(vulkanLib);
}
