////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/AMDVulkanMemoryAllocator.h"

#include "api/Vulkan.h"

#include "renderer/GPUBufferProperties.h"

namespace Reaper
{
struct ReaperRoot;

struct BufferInfo
{
    VkBuffer            buffer;
    GPUBufferProperties descriptor;
    VmaAllocation       allocation;
};

BufferInfo create_buffer(ReaperRoot& root, VkDevice device, const char* debug_string,
                         const GPUBufferProperties& properties, VmaAllocator& allocator);

void upload_buffer_data(VkDevice device, const VmaAllocator& allocator, const BufferInfo& buffer, const void* data,
                        std::size_t size);
} // namespace Reaper
