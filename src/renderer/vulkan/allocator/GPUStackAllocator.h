////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_GPUSTACKALLOCATOR_INCLUDED
#define REAPER_GPUSTACKALLOCATOR_INCLUDED

class GPUStackAllocator
{
public:
    GPUStackAllocator(std::size_t size);
    ~GPUStackAllocator();

public:
    std::size_t alloc(std::size_t size, std::size_t alignment);

private:
    std::size_t _size;
    std::size_t _offset;
};

#endif // REAPER_GPUSTACKALLOCATOR_INCLUDED
