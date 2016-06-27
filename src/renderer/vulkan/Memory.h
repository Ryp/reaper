////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MEMORY_INCLUDED
#define REAPER_MEMORY_INCLUDED

#include <vector>

#include "api/Vulkan.h"

struct MemoryTypeInfo
{
    bool deviceLocal = false;
    bool hostVisible = false;
    bool hostCoherent = false;
    bool hostCached = false;
    bool lazilyAllocated = false;

    struct Heap
    {
        uint64_t size = 0;
        bool deviceLocal = false;
    };

    Heap heap;
    int index;
};

std::vector<MemoryTypeInfo> enumerateHeaps(VkPhysicalDevice device);

VkDeviceMemory AllocateMemory(const std::vector<MemoryTypeInfo>& memoryInfos, VkDevice device, const int size);

VkBuffer AllocateBuffer(VkDevice device, const int size, const VkBufferUsageFlagBits bits);

u32 getMemoryType(VkPhysicalDevice device, uint32_t typeBits, VkFlags properties);

#endif // REAPER_MEMORY_INCLUDED
