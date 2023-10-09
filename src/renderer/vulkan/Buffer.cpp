////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Buffer.h"

#include "Debug.h"
#include "api/AssertHelper.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <algorithm>

namespace Reaper
{
namespace
{
    VkBufferUsageFlags BufferUsageToVulkan(u32 usageFlags)
    {
        VkBufferUsageFlags flags = 0;

        flags |= (usageFlags & GPUBufferUsage::TransferSrc) ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0;
        flags |= (usageFlags & GPUBufferUsage::TransferDst) ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
        flags |= (usageFlags & GPUBufferUsage::UniformTexelBuffer) ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : 0;
        flags |= (usageFlags & GPUBufferUsage::StorageTexelBuffer) ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : 0;
        flags |= (usageFlags & GPUBufferUsage::UniformBuffer) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0;
        flags |= (usageFlags & GPUBufferUsage::StorageBuffer) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
        flags |= (usageFlags & GPUBufferUsage::IndexBuffer) ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 0;
        flags |= (usageFlags & GPUBufferUsage::VertexBuffer) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0;
        flags |= (usageFlags & GPUBufferUsage::IndirectBuffer) ? VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT : 0;

        return flags;
    }
} // namespace

GPUBuffer create_buffer(VkDevice device, const char* debug_string, const GPUBufferProperties& input_properties,
                        VmaAllocator& allocator, MemUsage mem_usage)
{
    GPUBufferProperties properties = input_properties;

    // Uniform buffers require extra care on the CPU side.
    // There's a minimum buffer offset we need to take into account.
    // That means there's potentially extra padding between elements regardless
    // of the initial element size.
    if (properties.usage_flags & GPUBufferUsage::UniformBuffer)
    {
        // const u64 minUniformBufferOffsetAlignment =
        // backend.physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
        const u32 minUniformBufferOffsetAlignment = 0x40; // FIXME
        const u32 stride = std::max(properties.element_size_bytes, minUniformBufferOffsetAlignment);

        properties.stride = stride;
    }
    else
    {
        properties.stride = properties.element_size_bytes;
    }

    const VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                           .pNext = nullptr,
                                           .flags = VK_FLAGS_NONE,
                                           .size = properties.element_count * properties.stride,
                                           .usage = BufferUsageToVulkan(properties.usage_flags),
                                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                           .queueFamilyIndexCount = 0,
                                           .pQueueFamilyIndices = nullptr};

    VmaAllocationCreateInfo allocInfo = {};

    switch (mem_usage)
    {
    case MemUsage::GPU_Only:
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        break;
    case MemUsage::CPU_To_GPU:
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        break;
    case MemUsage::GPU_To_CPU:
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
        break;
    case MemUsage::CPU_Only:
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        break;
    default:
        AssertUnreachable();
        break;
    }

    VkBuffer      buffer;
    VmaAllocation allocation;
    Assert(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) == VK_SUCCESS);

    VulkanSetDebugName(device, buffer, debug_string);

    return GPUBuffer{
        .handle = buffer,
        .allocation = allocation,
        .properties_deprecated = properties,
    };
}

void upload_buffer_data(VkDevice device, const VmaAllocator& allocator, const GPUBuffer& buffer,
                        const GPUBufferProperties& buffer_properties, const void* data, std::size_t size,
                        u32 offset_elements)
{
    u8* writePtr = nullptr;

    Assert(size > 0, "Don't call this function with zero size");

    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(allocator, buffer.allocation, &allocation_info);

    const u64 offset_bytes = buffer_properties.stride * offset_elements;

    AssertVk(vkMapMemory(device, allocation_info.deviceMemory, allocation_info.offset + offset_bytes, size,
                         VK_FLAGS_NONE, reinterpret_cast<void**>(&writePtr)));

    Assert(size <= allocation_info.size,
           fmt::format("copy src of size {} on dst of size {}", size, allocation_info.size));

    if (buffer_properties.element_size_bytes == buffer_properties.stride)
    {
        memcpy(writePtr, data, size);
    }
    else
    {
        for (u32 i = 0; i < buffer_properties.element_count; i++)
        {
            const char* data_char = static_cast<const char*>(data);
            memcpy(writePtr + i * buffer_properties.stride, data_char + i * buffer_properties.element_size_bytes,
                   buffer_properties.element_size_bytes);
        }
    }

    vkUnmapMemory(device, allocation_info.deviceMemory);
}

void upload_buffer_data_deprecated(VkDevice device, const VmaAllocator& allocator, const GPUBuffer& buffer,
                                   const void* data, std::size_t size, u32 offset_elements)
{
    return upload_buffer_data(device, allocator, buffer, buffer.properties_deprecated, data, size, offset_elements);
}
} // namespace Reaper
