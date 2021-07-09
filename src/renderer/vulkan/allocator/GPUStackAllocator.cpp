////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GPUStackAllocator.h"

namespace Reaper
{
VirtualStackAllocator::VirtualStackAllocator(std::size_t size)
    : m_size(size)
    , m_offset(0)
{}

std::size_t VirtualStackAllocator::alloc(std::size_t size, std::size_t alignment)
{
    Assert(size > 0, "Invalid alloc size");
    Assert(alignment > 0, "Invalid alignment size");

    const std::size_t allocOffset = alignOffset(m_offset, alignment);

    m_offset = allocOffset + size;

    Assert(m_offset <= m_size, "Out of memory!");

    return allocOffset;
}

GPUStackAllocator::GPUStackAllocator(VkDeviceMemory memory, u32 index, std::size_t size)
    : m_allocator(size)
    , m_memory(memory)
    , m_memoryIndex(index)
{}

GPUAlloc GPUStackAllocator::alloc(VkMemoryRequirements requirements)
{
    Assert((m_memoryIndex & requirements.memoryTypeBits) > 0, "Incompatible memory type");

    const std::size_t offset = m_allocator.alloc(requirements.size, requirements.alignment);

    return GPUAlloc{m_memory, offset, requirements.size};
}
} // namespace Reaper
