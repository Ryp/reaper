////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Buffer.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/GPUBufferProperties.h"
#include "renderer/vulkan/Debug.h"

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

BufferInfo create_buffer(ReaperRoot& root, VkDevice device, const char* debug_string,
                         const GPUBufferProperties& properties, VmaAllocator& allocator)
{
    const VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                           nullptr,
                                           VK_FLAGS_NONE,
                                           properties.elementCount * properties.elementSize,
                                           BufferUsageToVulkan(properties.usageFlags),
                                           VK_SHARING_MODE_EXCLUSIVE,
                                           0,
                                           nullptr};

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkBuffer      buffer;
    VmaAllocation allocation;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

    log_debug(root, "vulkan: created buffer with handle: {}", static_cast<void*>(buffer));

    VulkanSetDebugName(device, buffer, debug_string);

    return {buffer, properties, allocation};
}

void upload_buffer_data(VkDevice device, const VmaAllocator& allocator, const BufferInfo& buffer, const void* data,
                        std::size_t size)
{
    u8* writePtr = nullptr;

    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(allocator, buffer.allocation, &allocation_info);

    Assert(vkMapMemory(device, allocation_info.deviceMemory, allocation_info.offset, allocation_info.size, 0,
                       reinterpret_cast<void**>(&writePtr))
           == VK_SUCCESS);

    Assert(size <= allocation_info.size,
           fmt::format("copy src of size {} on dst of size {}", size, allocation_info.size));

    memcpy(writePtr, data, size);

    vkUnmapMemory(device, allocation_info.deviceMemory);
}
} // namespace Reaper
