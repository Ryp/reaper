////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <nonstd/span.hpp>

namespace Reaper
{
namespace FrameGraph
{
    struct FrameGraph;
    struct FrameGraphSchedule;
    struct BarrierEvent;
} // namespace FrameGraph

struct FrameGraphResources;
struct CommandBuffer;

void record_framegraph_barriers(CommandBuffer& cmdBuffer, const FrameGraph::FrameGraphSchedule& schedule,
                                const FrameGraph::FrameGraph& framegraph, FrameGraphResources& resources,
                                nonstd::span<const FrameGraph::BarrierEvent> barriers, bool before);
} // namespace Reaper
