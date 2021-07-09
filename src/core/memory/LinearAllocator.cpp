////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "LinearAllocator.h"

#include <stdexcept>

LinearAllocator::LinearAllocator(std::size_t sizeBytes)
    : _memPtr(new char[sizeBytes])
    , _currentPtr(_memPtr)
    , _memSize(sizeBytes)
{}

LinearAllocator::~LinearAllocator()
{
    delete[] _memPtr;
}

void* LinearAllocator::alloc(std::size_t sizeBytes)
{
    void* ptr = _currentPtr;

    _currentPtr += sizeBytes;
    Assert(_currentPtr <= _memPtr + _memSize, "Out of memory!");
    return ptr;
}

void LinearAllocator::clear()
{
    _currentPtr = _memPtr;
}
