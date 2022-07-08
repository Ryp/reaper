////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameGraph.h"

#include <core/Assert.h>
#include <profiling/Scope.h>

#include <algorithm>

namespace Reaper::FrameGraph
{
namespace BarrierType
{
    const char* to_string(Type barrier_type)
    {
        switch (barrier_type)
        {
        case BarrierType::ImmediateBefore:
            return "ImmediateBefore";
        case BarrierType::ImmediateAfter:
            return "ImmediateAfter";
        case BarrierType::SplitBegin:
            return "SplitBegin";
        case BarrierType::SplitEnd:
            return "SplitEnd";
        default:
            AssertUnreachable();
            return "Invalid";
        }
    }
} // namespace BarrierType

const ResourceUsage& GetResourceUsage(const FrameGraph& framegraph, ResourceUsageHandle resourceUsageHandle)
{
    Assert(is_valid(resourceUsageHandle), "Invalid resource usage handle");
    return framegraph.ResourceUsages[resourceUsageHandle];
}

const Resource& GetResource(const FrameGraph& framegraph, const ResourceUsage& resourceUsage)
{
    const ResourceHandle resource_handle = resourceUsage.resource_handle;
    Assert(is_valid(resource_handle), "Invalid resource handle");

    if (resource_handle.is_texture)
        return framegraph.TextureResources[resource_handle.index];
    else
        return framegraph.BufferResources[resource_handle.index];
}

Resource& GetResource(FrameGraph& framegraph, const ResourceUsage& resourceUsage)
{
    const ResourceHandle resource_handle = resourceUsage.resource_handle;
    Assert(is_valid(resource_handle), "Invalid resource handle");

    if (resource_handle.is_texture)
        return framegraph.TextureResources[resource_handle.index];
    else
        return framegraph.BufferResources[resource_handle.index];
}

namespace
{
    // Thibault S. (29/11/2017) Yep it's recursive.
    bool HasCyclesRecursive(const DirectedAcyclicGraph&                    graph,
                            DirectedAcyclicGraph::index_type               nodeIndex,
                            std::vector<DirectedAcyclicGraph::index_type>& ancestors)
    {
        if (std::find(ancestors.begin(), ancestors.end(), nodeIndex) != ancestors.end())
            return true;

        ancestors.push_back(nodeIndex);
        for (const auto& childNodeIndex : graph.Nodes[nodeIndex].Children)
        {
            if (HasCyclesRecursive(graph, childNodeIndex, ancestors))
                return true;
        }
        ancestors.pop_back();
        return false;
    }
} // namespace

// Uses depth-first traversal
bool HasCycles(const DirectedAcyclicGraph& graph, const nonstd::span<DirectedAcyclicGraph::index_type> rootNodes)
{
    Assert(!rootNodes.empty(), "No root nodes were specified");
    std::vector<DirectedAcyclicGraph::index_type> ancestorStack;

    for (const auto& rootNode : rootNodes)
    {
        if (HasCyclesRecursive(graph, rootNode, ancestorStack))
            return true;
    }
    return false;
}

// Uses breadth-first traversal
void ComputeTransitiveClosure(const DirectedAcyclicGraph& graph,
                              nonstd::span<const DirectedAcyclicGraph::index_type>
                                                                             rootNodes,
                              std::vector<DirectedAcyclicGraph::index_type>& outClosure)
{
    REAPER_PROFILE_SCOPE_FUNC();

    const u32 nodeCount = graph.Nodes.size();

    Assert(nodeCount != 0, "Empty graph");
    Assert(nodeCount < 1000, "Big graph");
    Assert(!rootNodes.empty(), "No root nodes were specified");
    Assert(outClosure.empty(), "Non-empty closure output");

    // Keep track of visited nodes
    std::vector<bool> visitedNodes(nodeCount, false);
    // Fill(visitedNodes.begin(), visitedNodes.end(), false);

    // Initialize algorithm with root nodes
    // Thibault S. (22/08/2017) Copy PODs, 2017 edition. Zero cost abstraction right there...
    outClosure.insert(outClosure.begin(), rootNodes.begin(), rootNodes.end());

    // Breadth-first traversal
    for (u32 outClosureIndex = 0; outClosureIndex < outClosure.size(); outClosureIndex++) // Size is dynamic here
    {
        DirectedAcyclicGraph::index_type nodeIndex = outClosure[outClosureIndex];

        for (const auto& dependencyIndex : graph.Nodes[nodeIndex].Children)
        {
            Assert(dependencyIndex != nodeIndex, "This node self-loops");

            if (visitedNodes[dependencyIndex])
                continue;
            else
                outClosure.push_back(dependencyIndex);
        }
        visitedNodes[nodeIndex] = true;
    }
}

namespace
{
    void place_automatic_barriers(FrameGraphSchedule& schedule, const FrameGraph& framegraph)
    {
        // Indexed by resource handle
        const u32 texture_count = framegraph.TextureResources.size();
        const u32 buffer_count = framegraph.BufferResources.size();

        // Small trickery to treat all resource types with the same array
        std::vector<std::vector<ResourceUsageEvent>> per_resource_events(texture_count + buffer_count);

        // Append resource usage by scheduled execution order
        for (const auto& renderPassHandle : schedule.queue0)
        {
            const RenderPass& renderPass = framegraph.RenderPasses[renderPassHandle];

            for (const auto& resourceUsageHandle : renderPass.ResourceUsageHandles)
            {
                const ResourceUsage& resourceUsage = GetResourceUsage(framegraph, resourceUsageHandle);
                const ResourceHandle resourceHandle = resourceUsage.resource_handle;

                Assert(resourceUsage.is_used, "Accessing unused resource");

                std::vector<ResourceUsageEvent>& resource_events =
                    per_resource_events[resourceHandle.is_texture ? resourceHandle.index
                                                                  : resourceHandle.index + texture_count];

                // Assume that the resource WILL be created every frame and we need to transition it out of UNDEFINED
                // layout
                // FIXME This path is taken for buffers too since I'm too lazy to fix the .back() call just afterwards
                if (resource_events.empty())
                {
                    ResourceUsageEvent& initial_usage = resource_events.emplace_back();
                    initial_usage.render_pass = schedule.queue0[0];
                    initial_usage.usage_handle =
                        resourceUsageHandle; // FIXME it's wrong but it doesn't break the framegraph (yet)
                    initial_usage.access = {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                                            VK_IMAGE_LAYOUT_UNDEFINED};
                }

                ResourceUsageEvent&  previous_resource_event = resource_events.back();
                const ResourceUsage& previous_resource_usage =
                    GetResourceUsage(framegraph, previous_resource_event.usage_handle);

                // If we are in a multiple-reader situation, merge both accesses at earliest time
                if (previous_resource_usage.Type == UsageType::Input && resourceUsage.Type == UsageType::Input)
                {
                    const GPUResourceAccess old_access = previous_resource_usage.Usage.access;
                    const GPUResourceAccess new_access = resourceUsage.Usage.access;

                    Assert(!resourceHandle.is_texture || old_access.image_layout == new_access.image_layout,
                           "Using the same image layout is not supported for now");

                    // Patch access to accomodate both readers
                    previous_resource_event.access = {old_access.stage_mask | new_access.stage_mask,
                                                      old_access.access_mask | new_access.access_mask,
                                                      old_access.image_layout};
                }
                else
                {
                    ResourceUsageEvent& resource_event = resource_events.emplace_back();
                    resource_event.render_pass = renderPassHandle;
                    resource_event.usage_handle = resourceUsageHandle;
                    resource_event.access = resourceUsage.Usage.access;
                }
            }
        }

        // Build barriers now that we consolidated the successive accesses for each resource
        for (const auto& resource_events : per_resource_events)
        {
            for (u32 i = 1; i < resource_events.size(); i++)
            {
                const ResourceUsageEvent& src_resource_event = resource_events[i - 1];
                const ResourceUsageEvent& dst_resource_event = resource_events[i];

                const ResourceUsage& usage = GetResourceUsage(framegraph, dst_resource_event.usage_handle);
                Assert(!usage.resource_handle.is_texture
                           || src_resource_event.access.image_layout != dst_resource_event.access.image_layout,
                       "Mismatching image layout");

                Barrier& barrier = schedule.barriers.emplace_back();
                barrier.src = src_resource_event;
                barrier.dst = dst_resource_event;
            }
        }
    }
} // namespace

// NOTE: SUPER Trivial scheduling for now, matches user record order.
// We rely on the fact that render passes are appended in compatible rendering order, which saves our asses.
// NO fancy multiqueue stuff here. yet.
FrameGraphSchedule compute_schedule(const FrameGraph& framegraph)
{
    REAPER_PROFILE_SCOPE_FUNC();

    FrameGraphSchedule schedule;
    const u32          renderPassCount = framegraph.RenderPasses.size();

    for (u32 renderPassIndex = 0; renderPassIndex < renderPassCount; renderPassIndex++)
    {
        const RenderPass& renderPass = framegraph.RenderPasses[renderPassIndex];

        if (renderPass.is_used)
        {
            schedule.queue0.emplace_back(RenderPassHandle(renderPassIndex));
        }
    }

    place_automatic_barriers(schedule, framegraph);

    // We have the complete list of barriers to execute, now
    // let's build the timeline of commands to execute for each pass
    // This takes care of putting two events for a split barriers
    for (u32 i = 0; i < schedule.barriers.size(); i++)
    {
        const Barrier& barrier = schedule.barriers[i];

        // Test here if the render passes are consecutive. If so just do an immediate barrier
        // NOTE: This doesn't catch all cases of really consecutive passes
        const bool is_usage_immediate = (barrier.src.render_pass + 1 == barrier.dst.render_pass);

        if (is_usage_immediate)
        {
            schedule.barrier_events.emplace_back(BarrierEvent{BarrierType::ImmediateAfter, i, barrier.src.render_pass});
        }
        else
        {
            if (barrier.src.render_pass == barrier.dst.render_pass)
            {
                schedule.barrier_events.emplace_back(
                    BarrierEvent{BarrierType::ImmediateBefore, i, barrier.dst.render_pass});
            }
            else
            {
                schedule.barrier_events.emplace_back(BarrierEvent{BarrierType::SplitBegin, i, barrier.src.render_pass});
                schedule.barrier_events.emplace_back(BarrierEvent{BarrierType::SplitEnd, i, barrier.dst.render_pass});
            }
        }
    }

    // Sort first by renderpass and then by before/after marker
    auto comparison_less_lambda = [](BarrierEvent a, BarrierEvent b) -> bool {
        if (a.render_pass_handle == b.render_pass_handle)
        {
            const bool a_execute_before = (a.barrier_type & BarrierType::ExecuteBeforePass) > 0;
            const bool b_execute_after = (b.barrier_type & BarrierType::ExecuteAfterPass) > 0;

            return a_execute_before && b_execute_after;
        }

        return a.render_pass_handle < b.render_pass_handle;
    };

    // Sort barrier events so it's then trivial to gather them at runtime.
    // We could also just scatter them in arrays for each render passes and be done with it
    std::sort(schedule.barrier_events.begin(), schedule.barrier_events.end(), comparison_less_lambda);

    return schedule;
}

// This assumes the events are sorted by render pass
nonstd::span<const BarrierEvent> get_barriers_to_execute(const FrameGraphSchedule& schedule,
                                                         RenderPassHandle render_pass_handle, bool execute_before_pass)
{
    const BarrierEvent* begin = nullptr;
    const BarrierEvent* end = nullptr;

    for (u32 i = 0; i < schedule.barrier_events.size(); i++)
    {
        const BarrierEvent& barrier_event = schedule.barrier_events[i];
        const bool          match_order =
            execute_before_pass == ((barrier_event.barrier_type & BarrierType::ExecuteBeforePass) > 0);

        if (!begin && barrier_event.render_pass_handle == render_pass_handle && match_order)
        {
            begin = schedule.barrier_events.data() + i;
        }

        if (begin && barrier_event.render_pass_handle == render_pass_handle && match_order)
        {
            end = schedule.barrier_events.data() + i + 1;
        }
    }

    return nonstd::span(begin, end);
}
} // namespace Reaper::FrameGraph
