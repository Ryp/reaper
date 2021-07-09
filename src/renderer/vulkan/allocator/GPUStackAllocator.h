////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/api/Vulkan.h"

namespace Reaper
{
class VirtualStackAllocator
{
public:
    VirtualStackAllocator(std::size_t size);

public:
    std::size_t alloc(std::size_t size, std::size_t alignment);

private:
    std::size_t m_size;
    std::size_t m_offset;
};

struct GPUAlloc
{
    VkDeviceMemory memory;
    VkDeviceSize   offset;
    VkDeviceSize   size;
};

class GPUStackAllocator
{
public:
    GPUStackAllocator(VkDeviceMemory memory, u32 index, std::size_t size);

    GPUAlloc alloc(VkMemoryRequirements requirements);

private:
    VirtualStackAllocator m_allocator;
    VkDeviceMemory        m_memory;
    u32                   m_memoryIndex;
};
} // namespace Reaper
