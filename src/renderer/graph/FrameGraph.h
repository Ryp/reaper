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
    bool                             has_side_effects;
    std::vector<ResourceUsageHandle> ResourceUsageHandles;
    bool                             is_used;
};

union GPUResourceProperties
{
    GPUTextureProperties texture;
    GPUBufferProperties  buffer;
};

union GPUResourceView
{
    GPUTextureView texture;
    GPUBufferView  buffer;
};

struct GPUResourceAccess
{
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2        access_mask;
    VkImageLayout         image_layout;
};

struct Resource
{
    const char*           debug_name;
    GPUResourceProperties properties;
    bool                  is_used;
};

struct ResourceUsage
{
    u32                 type; // UsageType
    ResourceHandle      resource_handle;
    RenderPassHandle    render_pass;
    ResourceUsageHandle parent_usage_handle;
    GPUResourceAccess   access;
    GPUResourceView     view;
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
const Resource&      GetResource(const FrameGraph& framegraph, ResourceHandle resource_handle);
Resource&            GetResource(FrameGraph& framegraph, ResourceHandle resource_handle);
const Resource&      GetResource(const FrameGraph& framegraph, const ResourceUsage& resourceUsage);
Resource&            GetResource(FrameGraph& framegraph, const ResourceUsage& resourceUsage);

// Uses depth-first traversal
bool HasCycles(const DirectedAcyclicGraph& graph, nonstd::span<const DirectedAcyclicGraph::index_type> rootNodes);

// Uses breadth-first traversal
void ComputeTransitiveClosure(const DirectedAcyclicGraph& graph,
                              nonstd::span<const DirectedAcyclicGraph::index_type>
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
    enum Type
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

    const char* to_string(Type barrier_type);
} // namespace BarrierType

struct BarrierEvent
{
    BarrierType::Type barrier_type;
    u32               barrier_handle;
    RenderPassHandle  render_pass_handle;
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

inline GPUTextureAccess to_texture_access(GPUResourceAccess texture_access)
{
    return GPUTextureAccess{
        .stage_mask = texture_access.stage_mask,
        .access_mask = texture_access.access_mask,
        .image_layout = texture_access.image_layout,
    };
}

inline GPUBufferAccess to_buffer_access(GPUResourceAccess buffer_access)
{
    return GPUBufferAccess{
        .stage_mask = buffer_access.stage_mask,
        .access_mask = buffer_access.access_mask,
    };
}

inline GPUResourceAccess to_resource_access(GPUTextureAccess texture_access)
{
    return GPUResourceAccess{
        .stage_mask = texture_access.stage_mask,
        .access_mask = texture_access.access_mask,
        .image_layout = texture_access.image_layout,
    };
}

inline GPUResourceAccess to_resource_access(GPUBufferAccess buffer_access)
{
    return GPUResourceAccess{
        .stage_mask = buffer_access.stage_mask,
        .access_mask = buffer_access.access_mask,
        .image_layout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
}
} // namespace Reaper::FrameGraph
