#include "FrameGraphBuilder.h"

#include "GraphDebug.h"

#include <cstring>

namespace Reaper::FrameGraph
{
FrameGraphBuilder::FrameGraphBuilder(FrameGraph& frameGraph)
    : m_Graph(frameGraph)
{}

ResourceHandle FrameGraphBuilder::CreateResource(RenderPassHandle            renderPassHandle,
                                                 const char*                 identifier,
                                                 const GPUTextureProperties& descriptor)
{
    // TODO test for name clashes
    Resource& newResource = m_Graph.Resources.emplace_back();

    // Leave one extra char for null-termination
    std::strncpy(newResource.Identifier, identifier, Resource::MaxIdentifierLength - 1);
    newResource.Identifier[Resource::MaxIdentifierLength - 1] = '\0';

    newResource.Descriptor = descriptor;
    newResource.LifeBegin = renderPassHandle;
    newResource.LifeEnd = renderPassHandle;

    return ResourceHandle(m_Graph.Resources.size() - 1);
}

ResourceUsageHandle FrameGraphBuilder::CreateResourceUsage(UsageType           usageType,
                                                           RenderPassHandle    renderPassHandle,
                                                           ResourceHandle      resourceHandle,
                                                           GPUResourceUsage    textureUsage,
                                                           ResourceUsageHandle parentUsageHandle)
{
    ResourceUsage& newResourceUsage = m_Graph.ResourceUsages.emplace_back();

    Assert(resourceHandle != InvalidResourceHandle);
    newResourceUsage.Type = usageType;
    newResourceUsage.Resource = resourceHandle;
    newResourceUsage.RenderPass = renderPassHandle;
    newResourceUsage.Parent = parentUsageHandle;
    newResourceUsage.Usage = textureUsage;

    return ResourceUsageHandle(m_Graph.ResourceUsages.size() - 1);
}

RenderPassHandle FrameGraphBuilder::create_render_pass(const char* identifier, bool hasSideEffects)
{
    // TODO test for name clashes
    RenderPass& newRenderPass = m_Graph.RenderPasses.emplace_back();

    // Leave one extra char for null-termination
    strncpy(newRenderPass.Identifier, identifier, RenderPass::MaxNameLength - 1);
    newRenderPass.Identifier[RenderPass::MaxNameLength - 1] = '\0';

    newRenderPass.HasSideEffects = hasSideEffects;

    return RenderPassHandle(m_Graph.RenderPasses.size() - 1);
}

ResourceUsageHandle FrameGraphBuilder::create_texture(RenderPassHandle            renderPassHandle,
                                                      const char*                 name,
                                                      const GPUTextureProperties& resourceDesc,
                                                      GPUResourceUsage            usage)
{
    const ResourceHandle resourceHandle = CreateResource(renderPassHandle, name, resourceDesc);
    Assert(resourceHandle != InvalidResourceHandle);

    const ResourceUsageHandle resourceUsageHandle = CreateResourceUsage(
        UsageType::RenderPassOutput, renderPassHandle, resourceHandle, usage, InvalidResourceUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

ResourceUsageHandle FrameGraphBuilder::read_texture(RenderPassHandle    renderPassHandle,
                                                    ResourceUsageHandle inputUsageHandle,
                                                    GPUResourceUsage    usage)
{
    const ResourceHandle resourceHandle = GetResourceUsage(m_Graph, inputUsageHandle).Resource;
    Assert(resourceHandle != InvalidResourceHandle);

    const ResourceUsageHandle resourceUsageHandle =
        CreateResourceUsage(UsageType::RenderPassInput, renderPassHandle, resourceHandle, usage, inputUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

ResourceUsageHandle FrameGraphBuilder::WriteTexture(RenderPassHandle    renderPassHandle,
                                                    ResourceUsageHandle inputUsageHandle,
                                                    GPUResourceUsage    usage)
{
    // FIXME do something about the texture usage
    const GPUResourceUsage    readUsage = {};
    const ResourceUsageHandle readUsageHandle = read_texture(renderPassHandle, inputUsageHandle, readUsage);

    const ResourceHandle resourceHandle = GetResourceUsage(m_Graph, inputUsageHandle).Resource;
    Assert(resourceHandle != InvalidResourceHandle);

    const ResourceUsageHandle resourceUsageHandle =
        CreateResourceUsage(UsageType::RenderPassOutput, renderPassHandle, resourceHandle, usage, readUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

namespace
{
    // Thibault S. (07/02/2018) That's where the real magic happens!
    // By that i mean, this code is opaque AF, have fun debugging it :)
    void ConvertFrameGraphToDAG(const FrameGraph&                              frameGraph,
                                DirectedAcyclicGraph&                          outDAG,
                                std::vector<DirectedAcyclicGraph::index_type>& outRootNodes)
    {
        // Resource usage node indexes are the same for both graphs and
        // can be used interchangeably.
        for (const auto& resourceUsage : frameGraph.ResourceUsages)
        {
            (void)resourceUsage;
            outDAG.Nodes.emplace_back();
        }

        const u32 resourceUsageCount = frameGraph.ResourceUsages.size();
        const u32 renderPassCount = frameGraph.RenderPasses.size();

        // Renderpass node indexes are NOT the same for both graphs but
        // can be retrieved with an offset.
        for (const auto& renderPass : frameGraph.RenderPasses)
        {
            const DirectedAcyclicGraph::index_type nodeIndex = outDAG.Nodes.size();
            DirectedAcyclicGraph::node_type&       node = outDAG.Nodes.emplace_back();

            (void)node; // FIXME

            // Renderpasses that have side effects cannot be pruned
            // We use them as terminal nodes in the DAG so that orphan nodes
            // can properly be pruned later.
            if (renderPass.HasSideEffects)
                outRootNodes.push_back(nodeIndex);
        }

        // Check if the whole graph is likely to be pruned
        Assert(!outRootNodes.empty(), "No render passes were marked with the HasSideEffects flag.");

        // Convert edges
        for (u32 renderPassIndex = 0; renderPassIndex < renderPassCount; renderPassIndex++)
        {
            const RenderPass& renderPass = frameGraph.RenderPasses[renderPassIndex];
            for (const auto& resourceUsageHandle : renderPass.ResourceUsageHandles)
            {
                // Index conversion
                const DirectedAcyclicGraph::index_type renderPassIndexInDAG = renderPassIndex + resourceUsageCount;
                const DirectedAcyclicGraph::index_type resourceUsageIndexInDAG = resourceUsageHandle;

                const ResourceUsage& resourceUsage = GetResourceUsage(frameGraph, resourceUsageHandle);

                // Edges are directed in reversed compared to the rendering flow.
                if (resourceUsage.Type == UsageType::RenderPassInput)
                {
                    outDAG.Nodes[renderPassIndexInDAG].Children.push_back(resourceUsageIndexInDAG);

                    // Add extra edge to link to the previously written/created resource
                    Assert(resourceUsage.Parent != InvalidResourceUsageHandle);
                    DirectedAcyclicGraph::index_type parentUsageIndexInDAG = resourceUsage.Parent;

                    outDAG.Nodes[resourceUsageIndexInDAG].Children.push_back(parentUsageIndexInDAG);
                }
                else if (resourceUsage.Type == UsageType::RenderPassOutput)
                    outDAG.Nodes[resourceUsageIndexInDAG].Children.push_back(renderPassIndexInDAG);
            }
        }

        Assert(!HasCycles(outDAG, outRootNodes), "The framegraph has cycles");
    }

    // Thibault S. (08/02/2018)
    // This one is relatively simple, we recover infos from the DAG
    // to fill IsUsed fields.
    void FillFrameGraphUsedNodes(const nonstd::span<DirectedAcyclicGraph::index_type> usedNodes, FrameGraph& frameGraph)
    {
        // Reset the IsUsed flag
        for (auto& resourceUsage : frameGraph.ResourceUsages)
            resourceUsage.IsUsed = false;

        for (auto& resource : frameGraph.Resources)
            resource.IsUsed = false;

        for (auto& renderPass : frameGraph.RenderPasses)
            renderPass.IsUsed = false;

        // Be extra careful with terminalNodes indexes for the DAG representation,
        // again they are NOT the same as FrameGraph indexes.
        const u32 resourceCount = frameGraph.ResourceUsages.size();

        // Mark nodes in the framegraph as used or not
        for (auto nodeIndexInDAG : usedNodes)
        {
            // Index conversion
            if (nodeIndexInDAG < resourceCount)
            {
                ResourceUsage& resource = frameGraph.ResourceUsages[nodeIndexInDAG];
                resource.IsUsed = true;
            }
            else
            {
                RenderPass& renderPass = frameGraph.RenderPasses[nodeIndexInDAG - resourceCount];
                renderPass.IsUsed = true;
            }
        }

        for (const auto& renderPass : frameGraph.RenderPasses)
        {
            if (!renderPass.IsUsed)
                continue;

            for (auto resourceUsageHandle : renderPass.ResourceUsageHandles)
            {
                ResourceUsage& resourceUsage = frameGraph.ResourceUsages[resourceUsageHandle];

                if (resourceUsage.Type == UsageType::RenderPassOutput)
                {
                    resourceUsage.IsUsed = true;
                }
            }
        }

        for (auto& resourceUsage : frameGraph.ResourceUsages)
        {
            if (!resourceUsage.IsUsed)
                continue;

            Resource& resource = frameGraph.Resources[resourceUsage.Resource];
            resource.IsUsed = true;
        }
    }

    // Experimental deduction of resource lifetimes, useful for asserting
    // correct usage of the API and also allow efficient resource aliasing.
    //
    // NOTE:
    // There's a MASSIVE assumption here that renderpasses render in increasing
    // handle order (which is what currently happens normally).
    // That means that IF you want to reorder renderpasses (you will at one point)
    // you'll need to either do it before hand, or ditch the assumption and compute
    // lifetimes differently.
    void ComputeResourceLifetimes(FrameGraph& frameGraph)
    {
        const u32 renderPassCount = frameGraph.RenderPasses.size();

        for (u32 renderPassIndex = 0; renderPassIndex < renderPassCount; renderPassIndex++)
        {
            const RenderPassHandle renderPassHandle = RenderPassHandle(renderPassIndex);
            const RenderPass&      renderPass = frameGraph.RenderPasses[renderPassIndex];

            if (!renderPass.IsUsed)
                continue;

            for (const auto& resourceUsageHandle : renderPass.ResourceUsageHandles)
            {
                const ResourceUsage& resourceUsage = GetResourceUsage(frameGraph, resourceUsageHandle);
                Resource&            resource = GetResource(frameGraph, resourceUsage);

                Assert(resource.IsUsed);

                // Thibault S. (09/02/2018) LifeBegin is set while using
                // the builder API.
                Assert(renderPassHandle >= resource.LifeBegin);
                resource.LifeEnd = std::max(renderPassHandle, resource.LifeEnd);
            }
        }
    }
} // namespace

void FrameGraphBuilder::build()
{
    // Use the current graph to build an alternate representation
    // for easier processing.
    DirectedAcyclicGraph                          dag;
    std::vector<DirectedAcyclicGraph::index_type> rootNodes;
    ConvertFrameGraphToDAG(m_Graph, dag, rootNodes);

    // Compute the set of useful nodes with a flood fill from root nodes
    std::vector<DirectedAcyclicGraph::index_type> closure;
    ComputeTransitiveClosure(dag, rootNodes, closure);

    // Fill usage info from the DAG into the frame graph
    FillFrameGraphUsedNodes(closure, m_Graph);

    // Collecting lifetime info for resources will later allow us to efficiently
    // alias entity memory for resources.
    ComputeResourceLifetimes(m_Graph);
}
} // namespace Reaper::FrameGraph
