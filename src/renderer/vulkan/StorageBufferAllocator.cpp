////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "StorageBufferAllocator.h"

#include "Backend.h"
#include "Debug.h"
#include "api/AssertHelper.h"

namespace Reaper
{
StorageBufferAllocator create_storage_buffer_allocator(VulkanBackend& backend, const char* debug_name, u64 size_bytes)
{
    // NOTE: We rely on passing 1 byte as the element size to simplify math.
    GPUBufferProperties properties = DefaultGPUBufferProperties(size_bytes, 1, GPUBufferUsage::StorageBuffer);

    const VkBufferCreateInfo buffer_create_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = VK_FLAGS_NONE,
                                                   .size = size_bytes,
                                                   .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                   .queueFamilyIndexCount = 0,
                                                   .pQueueFamilyIndices = nullptr};

    // NOTE: Vulkan allows only one active mapping per VkMemory object. Since we use a persistent mapping we need to
    // flag it to VMA and get a separate memory object.
    // RANDOM should ideally be SEQUENTIAL but there's no strong guarantee that this happens with our API yet.
    const VmaAllocationCreateInfo allocation_create_info = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
                 | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = nullptr,
        .pUserData = nullptr,
        .priority = 0.f,
    };

    VkBuffer          buffer;
    VmaAllocation     allocation;
    VmaAllocationInfo allocation_info;
    AssertVk(vmaCreateBuffer(backend.vma_instance, &buffer_create_info, &allocation_create_info, &buffer, &allocation,
                             &allocation_info));

    VulkanSetDebugName(backend.device, buffer, debug_name);

    return StorageBufferAllocator{
        .buffer =
            {
                .handle = buffer,
                .allocation = allocation,
                .properties_deprecated = properties,
            },
        .properties = properties,
        .current_offset_bytes = 0,
        .mapped_ptr = allocation_info.pMappedData,
    };
}

void destroy_storage_buffer_allocator(VulkanBackend& backend, StorageBufferAllocator& allocator)
{
    vmaDestroyBuffer(backend.vma_instance, allocator.buffer.handle, allocator.buffer.allocation);
}

StorageBufferAlloc allocate_storage(StorageBufferAllocator& allocator, u64 size_bytes)
{
    const u64 current_offset = allocator.current_offset_bytes;
    allocator.current_offset_bytes += size_bytes;

    Assert(allocator.current_offset_bytes
           < allocator.properties.element_count * allocator.properties.element_size_bytes); // OOM

    return {
        .offset_bytes = current_offset,
        .size_bytes = size_bytes,
    };
}

void upload_storage_buffer(const StorageBufferAllocator& storage_allocator,
                           const StorageBufferAlloc&     storage_buffer_alloc,
                           const void*                   data_ptr)
{
    Assert(storage_buffer_alloc.size_bytes > 0, "Don't call this function with zero size");

    memcpy(static_cast<u8*>(storage_allocator.mapped_ptr) + storage_buffer_alloc.offset_bytes, data_ptr,
           storage_buffer_alloc.size_bytes);
}

void storage_allocator_commit_to_gpu(VulkanBackend& backend, StorageBufferAllocator& storage_allocator)
{
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(backend.vma_instance, storage_allocator.buffer.allocation, &allocation_info);

    const VkMappedMemoryRange range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .pNext = nullptr,
        .memory = allocation_info.deviceMemory,
        .offset = allocation_info.offset,
        .size = VK_WHOLE_SIZE,
    };

    vkInvalidateMappedMemoryRanges(backend.device, 1, &range);

    // Clear allocator offset
    storage_allocator.current_offset_bytes = 0;
}
} // namespace Reaper
