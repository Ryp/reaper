////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/AMDVulkanMemoryAllocator.h"

#include <vulkan_loader/Vulkan.h>

#include "renderer/buffer/GPUBufferProperties.h"

#include <core/Assert.h>

namespace Reaper
{
struct ReaperRoot;
struct BufferSubresource;

struct BufferInfo
{
    VkBuffer            handle;
    GPUBufferProperties properties;
    VmaAllocation       allocation;
};

enum class MemUsage
{
    GPU_Only,
    CPU_To_GPU,
    GPU_To_CPU,
    CPU_Only, // FIXME
};

BufferInfo create_buffer(ReaperRoot& root, VkDevice device, const char* debug_string,
                         const GPUBufferProperties& properties, VmaAllocator& allocator,
                         MemUsage mem_usage = MemUsage::GPU_Only);

void upload_buffer_data(VkDevice device, const VmaAllocator& allocator, const BufferInfo& buffer, const void* data,
                        std::size_t size, u32 offset_elements = 0);
} // namespace Reaper
