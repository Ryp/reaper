////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <span>

#include "renderer/graph/FrameGraph.h"

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
                                const FrameGraph::FrameGraph& framegraph, const FrameGraphResources& resources,
                                FrameGraph::RenderPassHandle render_pass_handle, bool before);

struct FrameGraphHelper
{
    const FrameGraph::FrameGraph&         frame_graph;
    const FrameGraph::FrameGraphSchedule& schedule;
    const FrameGraphResources&            resources;
};

class FrameGraphBarrierScope
{
public:
    explicit FrameGraphBarrierScope(CommandBuffer&               command_buffer,
                                    const FrameGraphHelper&      frame_graph_helper,
                                    FrameGraph::RenderPassHandle render_pass_handle)
        : m_command_buffer(command_buffer)
        , m_frame_graph_helper(frame_graph_helper)
        , m_render_pass_handle(render_pass_handle)
    {
        record_framegraph_barriers(m_command_buffer, m_frame_graph_helper.schedule, m_frame_graph_helper.frame_graph,
                                   m_frame_graph_helper.resources, m_render_pass_handle, true);
    }

    ~FrameGraphBarrierScope()
    {
        record_framegraph_barriers(m_command_buffer, m_frame_graph_helper.schedule, m_frame_graph_helper.frame_graph,
                                   m_frame_graph_helper.resources, m_render_pass_handle, false);
    }

private:
    CommandBuffer&               m_command_buffer;
    const FrameGraphHelper&      m_frame_graph_helper;
    FrameGraph::RenderPassHandle m_render_pass_handle;
};
} // namespace Reaper
