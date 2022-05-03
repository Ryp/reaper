#pragma once

#include "FrameGraphBasicTypes.h"

#include "renderer/buffer/GPUBufferProperties.h"
#include "renderer/buffer/GPUBufferView.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/texture/GPUTextureView.h"
#include "renderer/vulkan/Barrier.h"

#include <vector>

#include <nonstd/span.hpp>

namespace Reaper
{
enum class ELoadOp
{
    Load,
    Clear,
    DontCare
};

enum class EStoreOp
{
    Store,
    DontCare
};

enum class EImageLayout
{
    General,
    ColorAttachmentOptimal,
    DepthStencilAttachmentOptimal,
    DepthStencilReadOnlyOptimal,
    ShaderReadOnlyOptimal,
    TransferSrcOptimal,
    TransferDstOptimal,
    Preinitialized
};
} // namespace Reaper

namespace Reaper::FrameGraph
{
struct RenderPass
{
    const char*                      debug_name;
    bool                             HasSideEffects;
    std::vector<ResourceUsageHandle> ResourceUsageHandles;
    bool                             IsUsed;
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
    bool                  IsUsed;
    bool                  is_texture;
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
    UsageType           Type;
    ResourceHandle      Resource;
    RenderPassHandle    RenderPass;
    ResourceUsageHandle Parent;
    GPUResourceUsage    Usage;
    bool                IsUsed;
};

struct REAPER_RENDERER_API FrameGraph
{
    std::vector<ResourceUsage> ResourceUsages;
    std::vector<Resource>      Resources;
    std::vector<RenderPass>    RenderPasses;
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
ResourceHandle       GetResourceHandle(const FrameGraph& framegraph, ResourceUsageHandle resourceUsageHandle);
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

enum class BarrierType
{
    SingleBefore, // Executed BEFORE the renderpass
    SingleAfter,  // Executed AFTER the renderpass
    SplitBegin,   // Executed AFTER the renderpass
    SplitEnd,     // Executed BEFORE the renderpass
};

struct BarrierEvent
{
    BarrierType      type;
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
                                                         RenderPassHandle          render_pass_handle);
} // namespace Reaper::FrameGraph
