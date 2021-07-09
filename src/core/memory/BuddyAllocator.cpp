////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "BuddyAllocator.h"

#include <algorithm>
#include <memory>

#include "core/BitTricks.h"

static_assert(isPowerOfTwo(BuddyAllocator::DefaultMemoryAlignment), "npot alignment requirement");

namespace
{
inline std::size_t size_of_level(u32 level, std::size_t total_size)
{
    return total_size >> level;
}

inline u32 max_blocks_of_level(u32 level)
{
    return 1 << level;
}

inline u32 targetLevel(std::size_t allocSize, std::size_t maxLevelSize, u32 maxLevels)
{
    const std::size_t offset = bitOffset(allocSize - 1) + 1;
    const u32         level = static_cast<u32>(bitOffset(maxLevelSize) - offset);

    return std::min(maxLevels - 1, level);
}
} // namespace

BuddyAllocator::BuddyAllocator(std::size_t sizeBytes, std::size_t leafSizeBytes)
    : _memPtr(nullptr)
    , _memSize(0)
    , _alignedPtr(nullptr)
    , _alignedSize(0)
    , _levels(0)
{
    std::size_t allocSpace = sizeBytes + DefaultMemoryAlignment - 1;

    Assert(isPowerOfTwo(sizeBytes), "npot size");
    Assert(isPowerOfTwo(leafSizeBytes), "npot size");
    Assert(sizeBytes > leafSizeBytes, "allocator too small");

    _memPtr = new char[allocSpace];
    _memSize = allocSpace;

    _alignedSize = sizeBytes;
    _alignedPtr = static_cast<void*>(_memPtr);
    _alignedPtr = std::align(DefaultMemoryAlignment, sizeBytes, _alignedPtr, allocSpace);

    Assert(_alignedPtr != nullptr, "align failed");
    Assert(sizeBytes <= _alignedSize, "not enough memory");

    _levels = bitOffset(sizeBytes) - bitOffset(leafSizeBytes) + 1;

    _metaData.resize(bit(_levels));

    for (auto& meta : _metaData)
    {
        meta.free = false;
        meta.split = false;
    }
    _metaData[0].free = true;
}

BuddyAllocator::~BuddyAllocator()
{
    findLeaksRecursive(0);
    delete[] _memPtr;
}

void* BuddyAllocator::alloc(std::size_t sizeBytes)
{
    Assert(sizeBytes <= _alignedSize, "alloc out of bounds");

    const u32 level = targetLevel(sizeBytes, _alignedSize, _levels);
    const u32 blockIdx = allocBlock(level);

    std::size_t levelSize = size_of_level(level, _alignedSize);
    std::size_t offset = (blockIdx - ((1 << level) - 1)) * levelSize;
    char*       address = static_cast<char*>(_alignedPtr) + offset;

    Assert(address >= _alignedPtr, "return address is out of bounds");
    Assert(address < (static_cast<char*>(_alignedPtr) + _alignedSize), "return address is out of bounds");

    return address;
}

void BuddyAllocator::free(void* ptr, std::size_t sizeBytes)
{
    const u32   level = targetLevel(sizeBytes, _alignedSize, _levels);
    std::size_t offset = static_cast<char*>(ptr) - static_cast<char*>(_alignedPtr);
    const u32   blockIdx = static_cast<u32>(offset / size_of_level(level, _alignedSize) + ((1 << level) - 1));

    Assert(!_metaData[blockIdx].split, "trying to free a split block");
    Assert(!_metaData[blockIdx].free, "double free");

    _metaData[blockIdx].free = true;
    tryMergeFreeBlockRecursive(blockIdx);
}

u32 BuddyAllocator::allocBlock(u32 level)
{
    const u32 blocks = max_blocks_of_level(level);
    const u32 indexOffset = (1 << level) - 1;

    for (u32 index = 0; index < blocks; ++index)
    {
        if (_metaData[index + indexOffset].free)
        {
            _metaData[index + indexOffset].free = false;
            return index + indexOffset;
        }
    }
    Assert(level > 0, "out of memory");

    const u32 blockIdx = allocBlock(level - 1);

    _metaData[blockIdx].split = true;
    _metaData[blockIdx * 2 + 1].free = false;
    _metaData[blockIdx * 2 + 2].free = true;
    return blockIdx * 2 + 1;
}

void BuddyAllocator::tryMergeFreeBlockRecursive(u32 blockIdx)
{
    u32 buddyIdx = ((blockIdx - 1) ^ 1) + 1;
    u32 parentIdx = (blockIdx - 1) / 2;

    Assert(_metaData[blockIdx].free, "trying to merge an allocated block");
    Assert(!_metaData[blockIdx].split, "trying to merge from a split block");

    if (blockIdx > 0 && _metaData[buddyIdx].free)
    {
        Assert(_metaData[parentIdx].split, "trying to merge a non-split block");
        Assert(!_metaData[buddyIdx].split, "trying to merge with a split buddy");

        _metaData[parentIdx].split = false;
        _metaData[parentIdx].free = true;
        _metaData[blockIdx].free = false;
        _metaData[buddyIdx].free = false;
        tryMergeFreeBlockRecursive(parentIdx);
    }
}

void BuddyAllocator::findLeaksRecursive(u32 blockIdx)
{
    u32 child = blockIdx * 2 + 1;
    u32 maxBlockIdx = static_cast<u32>(_metaData.size() - 1);

    Assert(blockIdx < maxBlockIdx);

    if (_metaData[blockIdx].split && child < maxBlockIdx)
    {
        findLeaksRecursive(child);
        findLeaksRecursive(child + 1);
    }
    else
    {
        Assert(_metaData[blockIdx].free || blockIdx == 0, "block leaked");
    }
}
