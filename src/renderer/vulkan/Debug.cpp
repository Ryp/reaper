////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Debug.h"

#include "core/Assert.h"

#define REAPER_VULKAN_DEBUG REAPER_DEBUG

namespace Reaper
{
namespace
{
    template <typename HandleType>
    void SetDebugName(VkDevice device, VkObjectType handleType, HandleType handle, const char* name)
    {
        const VkDebugUtilsObjectNameInfoEXT debugNameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                                             nullptr, handleType, reinterpret_cast<u64>(handle), name};

#if REAPER_VULKAN_DEBUG
        Assert(vkSetDebugUtilsObjectNameEXT(device, &debugNameInfo) == VK_SUCCESS);
#endif
    }
} // namespace

// Use preprocessor to generate specializations
#define DEFINE_SET_DEBUG_NAME_SPECIALIZATION(vk_handle_type, vk_enum_value)           \
    void VulkanSetDebugName(VkDevice device, vk_handle_type handle, const char* name) \
    {                                                                                 \
        SetDebugName(device, vk_enum_value, handle, name);                            \
    }

DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkInstance, VK_OBJECT_TYPE_INSTANCE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkPhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkDevice, VK_OBJECT_TYPE_DEVICE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkQueue, VK_OBJECT_TYPE_QUEUE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkFence, VK_OBJECT_TYPE_FENCE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkDeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkBuffer, VK_OBJECT_TYPE_BUFFER)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkImage, VK_OBJECT_TYPE_IMAGE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkEvent, VK_OBJECT_TYPE_EVENT)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkRenderPass, VK_OBJECT_TYPE_RENDER_PASS)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkPipeline, VK_OBJECT_TYPE_PIPELINE)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkSampler, VK_OBJECT_TYPE_SAMPLER)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER)
DEFINE_SET_DEBUG_NAME_SPECIALIZATION(VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL)

// Command buffer helpers
void VulkanInsertDebugLabel(VkCommandBuffer commandBuffer, const char* name)
{
    // Color is supported but we're not using it
    const VkDebugUtilsLabelEXT debugLabelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, name, {}};

#if REAPER_VULKAN_DEBUG
    vkCmdInsertDebugUtilsLabelEXT(commandBuffer, &debugLabelInfo);
#endif
}

void VulkanBeginDebugLabel(VkCommandBuffer commandBuffer, const char* name)
{
    // Color is supported but we're not using it
    const VkDebugUtilsLabelEXT debugLabelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, name, {}};

#if REAPER_VULKAN_DEBUG
    vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabelInfo);
#endif
}

void VulkanEndDebugLabel(VkCommandBuffer commandBuffer)
{
#if REAPER_VULKAN_DEBUG
    vkCmdEndDebugUtilsLabelEXT(commandBuffer);
#endif
}

VulkanDebugLabelCmdBufferScope::VulkanDebugLabelCmdBufferScope(VkCommandBuffer commandBuffer, const char* name)
    : m_commandBuffer(commandBuffer)
{
    VulkanBeginDebugLabel(commandBuffer, name);
}

VulkanDebugLabelCmdBufferScope::~VulkanDebugLabelCmdBufferScope()
{
    VulkanEndDebugLabel(m_commandBuffer);
}

// Queue helpers
void VulkanInsertQueueDebugLabel(VkQueue queue, const char* name)
{
    // Color is supported but we're not using it
    const VkDebugUtilsLabelEXT debugLabelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, name, {}};

#if REAPER_VULKAN_DEBUG
    vkQueueInsertDebugUtilsLabelEXT(queue, &debugLabelInfo);
#endif
}

void VulkanBeginQueueDebugLabel(VkQueue queue, const char* name)
{
    // Color is supported but we're not using it
    const VkDebugUtilsLabelEXT debugLabelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, name, {}};

#if REAPER_VULKAN_DEBUG
    vkQueueBeginDebugUtilsLabelEXT(queue, &debugLabelInfo);
#endif
}

void VulkanEndQueueDebugLabel(VkQueue queue)
{
#if REAPER_VULKAN_DEBUG
    vkQueueEndDebugUtilsLabelEXT(queue);
#endif
}

VulkanQueueDebugLabelScope::VulkanQueueDebugLabelScope(VkQueue queue, const char* name)
    : m_queue(queue)
{
    VulkanBeginQueueDebugLabel(queue, name);
}

VulkanQueueDebugLabelScope::~VulkanQueueDebugLabelScope()
{
    VulkanEndQueueDebugLabel(m_queue);
}
} // namespace Reaper
