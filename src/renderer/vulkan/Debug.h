////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "api/Vulkan.h"

namespace Reaper
{
void VulkanSetDebugName(VkDevice device, VkDeviceMemory handle, const char* name);

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
