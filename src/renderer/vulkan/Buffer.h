////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/AMDVulkanMemoryAllocator.h"

#include "api/Vulkan.h"

#include "renderer/GPUBufferProperties.h"

namespace Reaper
{
struct ReaperRoot;

struct BufferInfo
{
    VkBuffer            buffer;
    GPUBufferProperties descriptor;
    VmaAllocation       allocation;
};

enum class MemUsage
{
    Default,
    CPU_Only, // FIXME
};

BufferInfo create_buffer(ReaperRoot& root, VkDevice device, const char* debug_string,
                         const GPUBufferProperties& properties, VmaAllocator& allocator,
                         MemUsage mem_usage = MemUsage::Default);

void upload_buffer_data(VkDevice device, const VmaAllocator& allocator, const BufferInfo& buffer, const void* data,
                        std::size_t size, u32 offset_elements = 0);

VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptorSet, u32 binding,
                                                    VkDescriptorType              descriptorType,
                                                    const VkDescriptorBufferInfo* bufferInfo);

VkDescriptorBufferInfo default_descriptor_buffer_info(const BufferInfo& bufferInfo);

struct GPUBufferView
{
    u32 elementOffset;
    u32 elementCount;
};

inline VkDescriptorBufferInfo get_vk_descriptor_buffer_info(const BufferInfo& bufferInfo, const GPUBufferView& view)
{
    Assert(bufferInfo.descriptor.stride > 0);

    return {
        bufferInfo.buffer,
        bufferInfo.descriptor.stride * view.elementOffset,
        bufferInfo.descriptor.stride * view.elementCount,
    };
}
} // namespace Reaper
