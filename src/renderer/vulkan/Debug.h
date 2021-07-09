////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "api/Vulkan.h"

namespace Reaper
{
// Use preprocessor to generate specializations
#define DECLARE_SET_DEBUG_NAME_SPECIALIZATION(vk_handle_type, vk_enum_value) \
    void VulkanSetDebugName(VkDevice device, vk_handle_type handle, const char* name);

DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkInstance, VK_OBJECT_TYPE_INSTANCE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkPhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkDevice, VK_OBJECT_TYPE_DEVICE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkQueue, VK_OBJECT_TYPE_QUEUE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkFence, VK_OBJECT_TYPE_FENCE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkDeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkBuffer, VK_OBJECT_TYPE_BUFFER)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkImage, VK_OBJECT_TYPE_IMAGE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkEvent, VK_OBJECT_TYPE_EVENT)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkRenderPass, VK_OBJECT_TYPE_RENDER_PASS)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkPipeline, VK_OBJECT_TYPE_PIPELINE)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkSampler, VK_OBJECT_TYPE_SAMPLER)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER)
DECLARE_SET_DEBUG_NAME_SPECIALIZATION(VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL)

void VulkanInsertDebugLabel(VkCommandBuffer commandBuffer, const char* name);
void VulkanBeginDebugLabel(VkCommandBuffer commandBuffer, const char* name);
void VulkanEndDebugLabel(VkCommandBuffer commandBuffer);

class VulkanDebugLabelCmdBufferScope
{
public:
    VulkanDebugLabelCmdBufferScope(VkCommandBuffer commandBuffer, const char* name);
    ~VulkanDebugLabelCmdBufferScope();

private:
    VkCommandBuffer m_commandBuffer;
};

void VulkanInsertQueueDebugLabel(VkQueue queue, const char* name);
void VulkanBeginQueueDebugLabel(VkQueue queue, const char* name);
void VulkanEndQueueDebugLabel(VkQueue queue);

class VulkanQueueDebugLabelScope
{
public:
    VulkanQueueDebugLabelScope(VkQueue queue, const char* name);
    ~VulkanQueueDebugLabelScope();

private:
    VkQueue m_queue;
};
} // namespace Reaper
