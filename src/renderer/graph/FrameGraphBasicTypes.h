////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <core/BitTricks.h>
#include <core/Types.h>

namespace Reaper::FrameGraph
{
namespace UsageType
{
    enum type : u32
    {
        Input = bit(0),
        Output = bit(1)
    };
}

// Kinda type-safe handle types used throughout
// the framegraph code.
enum RenderPassHandle : u32
{
};
enum ResourceUsageHandle : u32
{
};
struct ResourceHandle
{
    u32 index : 31;
    u32 is_texture : 1;
};

static constexpr RenderPassHandle    InvalidRenderPassHandle = RenderPassHandle(0xFFFFFFFF);
static constexpr ResourceUsageHandle InvalidResourceUsageHandle = ResourceUsageHandle(0xFFFFFFFF);
static constexpr ResourceHandle      InvalidResourceHandle = {.index = 0x7FFFFFFF, .is_texture = 1};

inline bool is_valid(RenderPassHandle handle)
{
    return handle != InvalidRenderPassHandle;
}
inline bool is_valid(ResourceUsageHandle handle)
{
    return handle != InvalidResourceUsageHandle;
}
inline bool is_valid(ResourceHandle handle)
{
    return handle.index != InvalidResourceHandle.index;
}
} // namespace Reaper::FrameGraph
