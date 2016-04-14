////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_STACKALLOCATOR_INCLUDED
#define REAPER_STACKALLOCATOR_INCLUDED

#include "Allocator.h"

class StackAllocator : public AbstractAllocator
{
public:
    StackAllocator(std::size_t sizeBytes);
    ~StackAllocator();

public:
    void*   alloc(std::size_t sizeBytes) override;

    void        freeToMarker(std::size_t marker);
    std::size_t getMarker() const;
    void        clear();

private:
    char*               _memPtr;
    const std::size_t   _memSize;
    std::size_t         _currentMarker;
};

#endif // REAPER_STACKALLOCATOR_INCLUDED
