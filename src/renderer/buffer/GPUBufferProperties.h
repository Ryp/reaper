////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "core/BitTricks.h"

namespace Reaper
{
namespace GPUBufferUsage
{
    enum Type : u32
    {
        TransferSrc = bit(0),
        TransferDst = bit(1),
        UniformTexelBuffer = bit(2),
        StorageTexelBuffer = bit(3),
        UniformBuffer = bit(4),
        StorageBuffer = bit(5),
        IndexBuffer = bit(6),
        VertexBuffer = bit(7),
        IndirectBuffer = bit(8),
    };
}

struct GPUBufferProperties
{
    u64 element_count;
    u32 element_size_bytes;
    u32 stride; // FIXME the API around the stride parameter is somewhat funky. Maybe there's nicer ways to abstract the
                // concept of discontinuous alignment.
    u32 usage_flags;
};

inline GPUBufferProperties DefaultGPUBufferProperties(u32 element_count, u32 element_size_bytes, u32 usage_flags)
{
    return GPUBufferProperties{element_count, element_size_bytes, element_size_bytes, usage_flags};
}
} // namespace Reaper
