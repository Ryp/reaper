////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "StackAllocator.h"

#include <stdexcept>

StackAllocator::StackAllocator(std::size_t sizeBytes)
    : _memPtr(new char[sizeBytes])
    , _memSize(sizeBytes)
    , _currentMarker(0)
{}

StackAllocator::~StackAllocator()
{
    delete[] _memPtr;
}

void* StackAllocator::alloc(std::size_t sizeBytes)
{
    void* ptr = _memPtr + _currentMarker;

    _currentMarker += sizeBytes;
    Assert(_currentMarker <= _memSize, "Out of memory!");
    return ptr;
}

void StackAllocator::freeToMarker(std::size_t marker)
{
    _currentMarker = marker;
}

std::size_t StackAllocator::getMarker() const
{
    return _currentMarker;
}

void StackAllocator::clear()
{
    _currentMarker = 0;
}
