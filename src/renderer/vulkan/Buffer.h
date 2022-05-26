////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/AMDVulkanMemoryAllocator.h"

#include "api/Vulkan.h"

#include "renderer/buffer/GPUBufferProperties.h"
#include "renderer/buffer/GPUBufferView.h"

namespace Reaper
{
struct ReaperRoot;
struct BufferSubresource;

struct BufferInfo
{
    VkBuffer            handle;
    GPUBufferProperties properties;
    VmaAllocation       allocation;
};

enum class MemUsage
{
    GPU_Only,
    CPU_To_GPU,
    GPU_To_CPU,
    CPU_Only, // FIXME
};

BufferInfo create_buffer(ReaperRoot& root, VkDevice device, const char* debug_string,
                         const GPUBufferProperties& properties, VmaAllocator& allocator,
                         MemUsage mem_usage = MemUsage::GPU_Only);

void upload_buffer_data(VkDevice device, const VmaAllocator& allocator, const BufferInfo& buffer, const void* data,
                        std::size_t size, u32 offset_elements = 0);

VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptorSet, u32 binding,
                                                    VkDescriptorType              descriptorType,
                                                    const VkDescriptorBufferInfo* bufferInfo);

VkDescriptorBufferInfo default_descriptor_buffer_info(const BufferInfo& bufferInfo);

inline VkDescriptorBufferInfo get_vk_descriptor_buffer_info(const BufferInfo&        bufferInfo,
                                                            const BufferSubresource& subresource)
{
    Assert(bufferInfo.properties.stride > 0);

    const GPUBufferView view = get_buffer_view(bufferInfo.properties, subresource);

    return {
        bufferInfo.handle,
        view.offset_bytes,
        view.size_bytes,
    };
}
} // namespace Reaper
