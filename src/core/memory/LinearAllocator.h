////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_LINEARALLOCATOR_INCLUDED
#define REAPER_LINEARALLOCATOR_INCLUDED

#include "Allocator.h"

class LinearAllocator : public AbstractAllocator
{
public:
    LinearAllocator(std::size_t sizeBytes);
    ~LinearAllocator();

public:
    void*   alloc(std::size_t sizeBytes) override;
    void    clear();

private:
    char*       _memPtr;
    char*       _currentPtr;
    std::size_t _memSize;
};

#endif // REAPER_LINEARALLOCATOR_INCLUDED
