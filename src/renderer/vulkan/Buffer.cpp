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
                         const GPUBufferProperties& properties, GPUStackAllocator& allocator)
{
    const VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                           nullptr,
                                           VK_FLAGS_NONE,
                                           properties.elementCount * properties.elementSize,
                                           BufferUsageToVulkan(properties.usageFlags),
                                           VK_SHARING_MODE_EXCLUSIVE,
                                           0,
                                           nullptr};

    VkBuffer buffer = VK_NULL_HANDLE;
    Assert(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) == VK_SUCCESS);

    VulkanSetDebugName(device, buffer, debug_string);

    // FIXME Debug usage
    log_debug(root, "vulkan: created buffer with handle: {}", static_cast<void*>(buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    log_debug(root, "- buffer memory requirements: size = {}, alignment = {}, types = {:#b}", memoryRequirements.size,
              memoryRequirements.alignment, memoryRequirements.memoryTypeBits);

    const GPUAlloc alloc = allocator.alloc(memoryRequirements);

    Assert(vkBindBufferMemory(device, buffer, alloc.memory, alloc.offset) == VK_SUCCESS);

    return {buffer, properties, alloc};
}

void upload_buffer_data(VkDevice device, const BufferInfo& buffer, const void* data, std::size_t size)
{
    u8* writePtr = nullptr;

    Assert(vkMapMemory(device, buffer.alloc.memory, buffer.alloc.offset, buffer.alloc.size, 0,
                       reinterpret_cast<void**>(&writePtr))
           == VK_SUCCESS);

    Assert(size <= buffer.alloc.size, fmt::format("copy src of size {} on dst of size {}", size, buffer.alloc.size));

    memcpy(writePtr, data, size);

    vkUnmapMemory(device, buffer.alloc.memory);
}
} // namespace Reaper
