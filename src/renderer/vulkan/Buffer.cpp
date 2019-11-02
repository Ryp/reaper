////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Buffer.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

namespace Reaper
{
namespace
{
    BufferInfo create_buffer(ReaperRoot& root, VkDevice device, std::size_t elementCount, std::size_t elementSize,
                             GPUStackAllocator& allocator, VkBufferUsageFlags usage)
    {
        const VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                               nullptr,
                                               VK_FLAGS_NONE,
                                               elementCount * elementSize,
                                               usage,
                                               VK_SHARING_MODE_EXCLUSIVE,
                                               0,
                                               nullptr};

        VkBuffer buffer = VK_NULL_HANDLE;
        Assert(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) == VK_SUCCESS);

        // FIXME Debug usage
        log_debug(root, "vulkan: created buffer with handle: {}", static_cast<void*>(buffer));

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

        log_debug(root, "- buffer memory requirements: size = {}, alignment = {}, types = {:#b}",
                  memoryRequirements.size, memoryRequirements.alignment, memoryRequirements.memoryTypeBits);

        const GPUAlloc alloc = allocator.alloc(memoryRequirements);

        Assert(vkBindBufferMemory(device, buffer, alloc.memory, alloc.offset) == VK_SUCCESS);

        return {buffer, alloc};
    }
} // namespace

BufferInfo create_vertex_buffer(ReaperRoot& root, VkDevice device, std::size_t elementCount, std::size_t elementSize,
                                GPUStackAllocator& allocator)
{
    return create_buffer(root, device, elementCount, elementSize, allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

BufferInfo create_index_buffer(ReaperRoot& root, VkDevice device, std::size_t elementCount, std::size_t elementSize,
                               GPUStackAllocator& allocator)
{
    return create_buffer(root, device, elementCount, elementSize, allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

BufferInfo create_constant_buffer(ReaperRoot& root, VkDevice device, std::size_t size, GPUStackAllocator& allocator)
{
    return create_buffer(root, device, 1, size, allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
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
