////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2018 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/GPUStackAllocator.h"
#include "api/Vulkan.h"

namespace Reaper
{
struct ReaperRoot;

struct BufferInfo
{
    VkBuffer buffer;
    GPUAlloc alloc;
};

BufferInfo create_vertex_buffer(ReaperRoot& root, VkDevice device, std::size_t elementCount, std::size_t elementSize,
                                GPUStackAllocator& allocator);

BufferInfo create_index_buffer(ReaperRoot& root, VkDevice device, std::size_t elementCount, std::size_t elementSize,
                               GPUStackAllocator& allocator);

BufferInfo create_constant_buffer(ReaperRoot& root, VkDevice device, std::size_t size, GPUStackAllocator& allocator);

void upload_buffer_data(VkDevice device, const BufferInfo& buffer, const void* data, std::size_t size);
} // namespace Reaper
