////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Buffer.h"

namespace Reaper
{
struct StorageBufferAllocator
{
    GPUBuffer           buffer;
    GPUBufferProperties properties;
    u64                 current_offset_bytes;
    void*               mapped_ptr;
};

struct VulkanBackend;

StorageBufferAllocator create_storage_buffer_allocator(VulkanBackend& backend, const char* debug_name, u64 size_bytes);
void                   destroy_storage_buffer_allocator(VulkanBackend& backend, StorageBufferAllocator& allocator);

struct StorageBufferAlloc
{
    u64 offset_bytes;
    u64 size_bytes;
};

// FIXME Alignment is not handled at all at this time
// You should get a validation error when we need to properly support this
StorageBufferAlloc allocate_storage(StorageBufferAllocator& allocator, u64 size_bytes);

void upload_storage_buffer(const StorageBufferAllocator& storage_allocator,
                           const StorageBufferAlloc&     storage_buffer_alloc,
                           const void*                   data_ptr);

void storage_allocator_commit_to_gpu(VulkanBackend& backend, StorageBufferAllocator& storage_allocator);
} // namespace Reaper
