#pragma once

#include "FrameGraphBasicTypes.h"

#include "renderer/texture/GPUTextureProperties.h"

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
    static constexpr u32 MaxNameLength = 64;

    char                             Identifier[MaxNameLength];
    bool                             HasSideEffects;
    std::vector<ResourceUsageHandle> ResourceUsageHandles;
    bool                             IsUsed;
};

struct Resource
{
    static constexpr u32 MaxIdentifierLength = 64;

    char                 Identifier[MaxIdentifierLength];
    GPUTextureProperties Descriptor;
    EntityHandle         entity_handle;
    RenderPassHandle     LifeBegin;
    RenderPassHandle     LifeEnd;
    bool                 IsUsed;
};

struct TGPUTextureUsage // FIXME
{
    ELoadOp      LoadOp;
    EStoreOp     StoreOp;
    EImageLayout Layout;
};

struct ResourceUsage
{
    UsageType           Type;
    ResourceHandle      Resource;
    ResourceUsageHandle Parent;
    TGPUTextureUsage    Usage;
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
const Resource&      GetResource(const FrameGraph& framegraph, const ResourceUsage& resourceUsage);
Resource&            GetResource(FrameGraph& framegraph, const ResourceUsage& resourceUsage);

void VerifyResourceLifetime(const Resource& resource, const RenderPassHandle renderPassHandle);

// Uses depth-first traversal
bool HasCycles(const DirectedAcyclicGraph& graph, const nonstd::span<DirectedAcyclicGraph::index_type> rootNodes);

// Uses breadth-first traversal
void ComputeTransitiveClosure(const DirectedAcyclicGraph& graph,
                              const nonstd::span<DirectedAcyclicGraph::index_type>
                                                                             rootNodes,
                              std::vector<DirectedAcyclicGraph::index_type>& outClosure);
} // namespace Reaper::FrameGraph
