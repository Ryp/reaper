////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
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
    u32 elementCount;
    u32 elementSize;
    u32 usageFlags;
};

inline GPUBufferProperties DefaultGPUBufferProperties(u32 elementCount, u32 elementSize, u32 usageFlags)
{
    return GPUBufferProperties{elementCount, elementSize, usageFlags};
}
} // namespace Reaper
