////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "FrameGraphBasicTypes.h"

#include "renderer/buffer/GPUBufferProperties.h"
#include "renderer/buffer/GPUBufferView.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/texture/GPUTextureView.h"
#include "renderer/vulkan/Barrier.h"

#include <vector>

#include <nonstd/span.hpp>

namespace Reaper::FrameGraph
{
struct RenderPass
{
    const char*                      debug_name;
    bool                             HasSideEffects;
    std::vector<ResourceUsageHandle> ResourceUsageHandles;
    bool                             is_used;
};

union GPUResourceProperties
{
    GPUTextureProperties texture;
    GPUBufferProperties  buffer;
};

struct Resource
{
    const char*           debug_name;
    GPUResourceProperties properties;
    bool                  is_used;
};

struct GPUResourceUsage
{
    GPUResourceAccess access;
    union
    {
        GPUTextureView texture_view;
        GPUBufferView  buffer_view;
    };
};

struct ResourceUsage
{
    u32                 Type; // UsageType
    ResourceHandle      resource_handle;
    RenderPassHandle    RenderPass;
    ResourceUsageHandle parent_usage_handle;
    GPUResourceUsage    Usage;
    bool                is_used;
};

struct REAPER_RENDERER_API FrameGraph
{
    std::vector<RenderPass>    RenderPasses;
    std::vector<ResourceUsage> ResourceUsages;
    std::vector<Resource>      BufferResources;
    std::vector<Resource>      TextureResources;
};

struct DirectedAcyclicGraph
{
    using index_type = u32;
    struct node_type
    {
        std::vector<index_type> Children;
    };

    std::vector<node_type> Nodes;
};

const ResourceUsage& GetResourceUsage(const FrameGraph& framegraph, ResourceUsageHandle resourceUsageHandle);
const Resource&      GetResource(const FrameGraph& framegraph, const ResourceUsage& resourceUsage);
Resource&            GetResource(FrameGraph& framegraph, const ResourceUsage& resourceUsage);

// Uses depth-first traversal
bool HasCycles(const DirectedAcyclicGraph& graph, const nonstd::span<DirectedAcyclicGraph::index_type> rootNodes);

// Uses breadth-first traversal
void ComputeTransitiveClosure(const DirectedAcyclicGraph& graph,
                              const nonstd::span<DirectedAcyclicGraph::index_type>
                                                                             rootNodes,
                              std::vector<DirectedAcyclicGraph::index_type>& outClosure);

struct ResourceUsageEvent
{
    RenderPassHandle    render_pass;
    ResourceUsageHandle usage_handle;
    GPUResourceAccess   access;
};

struct Barrier
{
    ResourceUsageEvent src;
    ResourceUsageEvent dst;
};

namespace BarrierType
{
    // Barriers are tied to a render pass for now, so 'before' and 'after' relate to that concept
    enum type : u8
    {
        Immediate = bit(0),
        Split = bit(1),
        ExecuteBeforePass = bit(2),
        ExecuteAfterPass = bit(3),

        ImmediateAfter = Immediate | ExecuteAfterPass,
        ImmediateBefore = Immediate | ExecuteBeforePass,
        SplitBegin = Split | ExecuteAfterPass,
        SplitEnd = Split | ExecuteBeforePass,
    };
} // namespace BarrierType

struct BarrierEvent
{
    u32              type;
    u32              barrier_handle;
    RenderPassHandle render_pass_handle;
};

struct FrameGraphSchedule
{
    std::vector<RenderPassHandle> queue0;
    std::vector<Barrier>          barriers;
    std::vector<BarrierEvent>     barrier_events;
};

FrameGraphSchedule compute_schedule(const FrameGraph& framegraph);

nonstd::span<const BarrierEvent> get_barriers_to_execute(const FrameGraphSchedule& schedule,
                                                         RenderPassHandle render_pass_handle, bool execute_before_pass);
} // namespace Reaper::FrameGraph
