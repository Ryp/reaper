////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Debug.h"

#include "core/Assert.h"

namespace Reaper
{
namespace
{
    template <typename HandleType>
    void SetDebugName(VkDevice device, VkObjectType handleType, HandleType handle, const char* name)
    {
        const VkDebugUtilsObjectNameInfoEXT debugNameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                                             nullptr, handleType, reinterpret_cast<u64>(handle), name};

        Assert(vkSetDebugUtilsObjectNameEXT(device, &debugNameInfo) == VK_SUCCESS);
    }
} // namespace

void VulkanSetDebugName(VkDevice device, VkDeviceMemory handle, const char* name)
{
    SetDebugName(device, VK_OBJECT_TYPE_DEVICE_MEMORY, handle, name);
}

// Command buffer helpers
void VulkanInsertDebugLabel(VkCommandBuffer commandBuffer, const char* name)
{
    // Color is supported but we're not using it
    const VkDebugUtilsLabelEXT debugLabelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, name, {}};

    vkCmdInsertDebugUtilsLabelEXT(commandBuffer, &debugLabelInfo);
}

void VulkanBeginDebugLabel(VkCommandBuffer commandBuffer, const char* name)
{
    // Color is supported but we're not using it
    const VkDebugUtilsLabelEXT debugLabelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, name, {}};

    vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabelInfo);
}

void VulkanEndDebugLabel(VkCommandBuffer commandBuffer)
{
    vkCmdEndDebugUtilsLabelEXT(commandBuffer);
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

    vkQueueInsertDebugUtilsLabelEXT(queue, &debugLabelInfo);
}

void VulkanBeginQueueDebugLabel(VkQueue queue, const char* name)
{
    // Color is supported but we're not using it
    const VkDebugUtilsLabelEXT debugLabelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, name, {}};

    vkQueueBeginDebugUtilsLabelEXT(queue, &debugLabelInfo);
}

void VulkanEndQueueDebugLabel(VkQueue queue)
{
    vkQueueEndDebugUtilsLabelEXT(queue);
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
