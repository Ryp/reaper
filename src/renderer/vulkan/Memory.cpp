////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Memory.h"

using namespace vk;

std::vector<MemoryTypeInfo> enumerateHeaps(VkPhysicalDevice device)
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

    std::vector<MemoryTypeInfo::Heap> heaps;

    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
    {
        MemoryTypeInfo::Heap info;
        info.size = memoryProperties.memoryHeaps[i].size;
        info.deviceLocal = (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;

        heaps.push_back(info);
    }

    std::vector<MemoryTypeInfo> result;

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        MemoryTypeInfo typeInfo;

        typeInfo.deviceLocal =
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        typeInfo.hostVisible =
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        typeInfo.hostCoherent =
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        typeInfo.hostCached = (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;
        typeInfo.lazilyAllocated =
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0;

        typeInfo.heap = heaps[memoryProperties.memoryTypes[i].heapIndex];

        typeInfo.index = static_cast<int>(i);

        result.push_back(typeInfo);
    }

    return result;
}

VkDeviceMemory AllocateMemory(const std::vector<MemoryTypeInfo>& memoryInfos, VkDevice device, const int size)
{
    // We take the first HOST_VISIBLE memory
    for (auto& memoryInfo : memoryInfos)
    {
        if (memoryInfo.hostVisible)
        {
            VkMemoryAllocateInfo memoryAllocateInfo = {};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memoryAllocateInfo.memoryTypeIndex = memoryInfo.index;
            memoryAllocateInfo.allocationSize = size;

            VkDeviceMemory deviceMemory;
            vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceMemory);
            return deviceMemory;
        }
    }

    return VK_NULL_HANDLE;
}

u32 getMemoryType(VkPhysicalDevice device, uint32_t typeBits, VkMemoryPropertyFlags requiredProperties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    {
        if ((typeBits & 1) == 1)
        {
            if ((memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties)
            {
                return i;
            }
        }
        typeBits >>= 1;
    }
    AssertUnreachable();
    return 0;
}
