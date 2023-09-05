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

struct GPUBuffer
{
    VkBuffer            handle;
    VmaAllocation       allocation;
    GPUBufferProperties properties_deprecated; // FIXME
};

enum class MemUsage
{
    GPU_Only,
    CPU_To_GPU,
    GPU_To_CPU,
    CPU_Only, // FIXME
};

GPUBuffer create_buffer(VkDevice device, const char* debug_string, const GPUBufferProperties& properties,
                        VmaAllocator& allocator, MemUsage mem_usage = MemUsage::GPU_Only);

void upload_buffer_data(VkDevice device, const VmaAllocator& allocator, const GPUBuffer& buffer,
                        const GPUBufferProperties& buffer_properties, const void* data, std::size_t size,
                        u32 offset_elements = 0);
void upload_buffer_data_deprecated(VkDevice device, const VmaAllocator& allocator, const GPUBuffer& buffer,
                                   const void* data, std::size_t size, u32 offset_elements = 0);
} // namespace Reaper
