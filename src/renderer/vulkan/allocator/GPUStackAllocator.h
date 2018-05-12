////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2018 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

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
