#include "FrameGraph.h"

namespace Reaper::FrameGraph
{
void VerifyResourceLifetime(const Resource& resource, const RenderPassHandle renderPassHandle)
{
    Assert(resource.LifeBegin != InvalidRenderPassHandle, "Invalid lifetime begin");
    Assert(resource.LifeEnd != InvalidRenderPassHandle, "Invalid lifetime end");
    Assert(renderPassHandle >= resource.LifeBegin, "The resource is not accessible yet");
    Assert(renderPassHandle <= resource.LifeEnd, "The resource is not accessible anymore");
}

const ResourceUsage& GetResourceUsage(const FrameGraph& framegraph, ResourceUsageHandle resourceUsageHandle)
{
    Assert(resourceUsageHandle != InvalidResourceUsageHandle, "Invalid resource usage handle");
    return framegraph.ResourceUsages[resourceUsageHandle];
}

const Resource& GetResource(const FrameGraph& framegraph, const ResourceUsage& resourceUsage)
{
    Assert(resourceUsage.Resource != InvalidResourceHandle, "Invalid resource handle");
    return framegraph.Resources[resourceUsage.Resource];
}

Resource& GetResource(FrameGraph& framegraph, const ResourceUsage& resourceUsage)
{
    Assert(resourceUsage.Resource != InvalidResourceHandle, "Invalid resource handle");
    return framegraph.Resources[resourceUsage.Resource];
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
                              const nonstd::span<DirectedAcyclicGraph::index_type>
                                                                             rootNodes,
                              std::vector<DirectedAcyclicGraph::index_type>& outClosure)
{
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
} // namespace Reaper::FrameGraph
