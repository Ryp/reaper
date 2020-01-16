#pragma once

namespace Reaper::FrameGraph
{
enum class UsageType
{
    RenderPassInput,
    RenderPassOutput
};

// Kinda type-safe handle types used throughout
// the framegraph code.
enum RenderPassHandle : u32
{
};
enum ResourceUsageHandle : u32
{
};
enum ResourceHandle : u32
{
};
enum EntityHandle : u32
{
};

static constexpr RenderPassHandle    InvalidRenderPassHandle = RenderPassHandle(0xFFFFFFFF);
static constexpr ResourceUsageHandle InvalidResourceUsageHandle = ResourceUsageHandle(0xFFFFFFFF);
static constexpr ResourceHandle      InvalidResourceHandle = ResourceHandle(0xFFFFFFFF);
static constexpr EntityHandle        InvalidEntityHandle = EntityHandle(0xFFFFFFFF);
} // namespace Reaper::FrameGraph
