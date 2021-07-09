////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Allocator.h"

#include <vector>

// http://bitsquid.blogspot.fr/2015/08/allocation-adventures-3-buddy-allocator.html

class REAPER_CORE_API BuddyAllocator : public AbstractAllocator
{
    struct MetaData
    {
        bool free;
        bool split;
    };

public:
    static constexpr std::size_t DefaultMemoryAlignment = 16;
    static constexpr int         DefaultLeafSize = 64;
    static constexpr int         MaxLevels = 32;

public:
    BuddyAllocator(std::size_t sizeBytes, std::size_t leafSizeBytes = DefaultLeafSize);
    ~BuddyAllocator();

    BuddyAllocator(const BuddyAllocator& other) = delete;
    BuddyAllocator& operator=(const BuddyAllocator& other) = delete;

public:
    void* alloc(std::size_t sizeBytes) override;
    void  free(void* ptr, std::size_t sizeBytes);

private:
    u32  allocBlock(u32 level);
    void tryMergeFreeBlockRecursive(u32 blockIdx);
    void findLeaksRecursive(u32 blockIdx);

private:
    char*       _memPtr;
    std::size_t _memSize;
    void*       _alignedPtr;
    std::size_t _alignedSize;
    u32         _levels;

    std::vector<MetaData> _metaData;
};
