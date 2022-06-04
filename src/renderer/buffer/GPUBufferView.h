////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <core/Types.h>

namespace Reaper
{
struct GPUBufferProperties;

struct BufferSubresource
{
    u64 element_offset;
    u64 element_count;
};

struct GPUBufferView
{
    u64 offset_bytes;
    u64 size_bytes;
};

GPUBufferView default_buffer_view(const GPUBufferProperties& properties);

GPUBufferView get_buffer_view(const GPUBufferProperties& properties, const BufferSubresource& subresource);
} // namespace Reaper
