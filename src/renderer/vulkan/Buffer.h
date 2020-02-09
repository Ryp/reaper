////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "allocator/GPUStackAllocator.h"

#include "api/Vulkan.h"

#include "renderer/GPUBufferProperties.h"

namespace Reaper
{
struct ReaperRoot;

struct BufferInfo
{
    VkBuffer            buffer;
    GPUBufferProperties descriptor;
    GPUAlloc            alloc;
};

BufferInfo create_buffer(ReaperRoot& root, VkDevice device, const char* debug_string,
                         const GPUBufferProperties& properties, GPUStackAllocator& allocator);

void upload_buffer_data(VkDevice device, const BufferInfo& buffer, const void* data, std::size_t size);
} // namespace Reaper
