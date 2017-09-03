////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GPUStackAllocator.h"

GPUStackAllocator::GPUStackAllocator(std::size_t size)
    : _size(size)
    , _offset(0)
{
}

GPUStackAllocator::~GPUStackAllocator()
{
}

std::size_t GPUStackAllocator::alloc(std::size_t size, std::size_t alignment)
{
    std::size_t allocOffset = alignOffset(_offset, alignment);

    _offset = allocOffset + size;

    Assert(size > 0, "Invalid alloc size");
    Assert(alignment > 0, "Invalid alignment size");
    Assert(_offset <= _size, "Out of memory!");

    return allocOffset;
}
