#include "FrameGraphBuilder.h"

#include "GraphDebug.h"

#include <cstring>

namespace Reaper::FrameGraph
{
Builder::Builder(FrameGraph& frameGraph)
    : m_Graph(frameGraph)
{}

ResourceHandle Builder::create_resource(const char* debug_name, const GPUResourceProperties& properties,
                                        bool is_texture)
{
    // TODO test for name clashes
    Resource& newResource = m_Graph.Resources.emplace_back();
    newResource.debug_name = debug_name;
    newResource.properties = properties;
    newResource.is_texture = is_texture;

    return ResourceHandle(m_Graph.Resources.size() - 1);
}

ResourceUsageHandle Builder::create_resource_usage(UsageType           usageType,
                                                   RenderPassHandle    renderPassHandle,
                                                   ResourceHandle      resourceHandle,
                                                   GPUResourceUsage    resourceUsage,
                                                   ResourceUsageHandle parentUsageHandle)
{
    ResourceUsage& newResourceUsage = m_Graph.ResourceUsages.emplace_back();

    Assert(resourceHandle != InvalidResourceHandle, "Invalid resource handle");
    newResourceUsage.Type = usageType;
    newResourceUsage.Resource = resourceHandle;
    newResourceUsage.RenderPass = renderPassHandle;
    newResourceUsage.Parent = parentUsageHandle;
    newResourceUsage.Usage = resourceUsage;

    return ResourceUsageHandle(m_Graph.ResourceUsages.size() - 1);
}

ResourceUsageHandle Builder::create_resource_generic(RenderPassHandle renderPassHandle, const char* name,
                                                     const GPUResourceProperties& properties,
                                                     GPUResourceUsage resource_usage, bool is_texture)
{
    const ResourceHandle resourceHandle = create_resource(name, properties, is_texture);
    Assert(resourceHandle != InvalidResourceHandle, "Invalid resource handle");

    const ResourceUsageHandle resourceUsageHandle = create_resource_usage(
        UsageType::RenderPassOutput, renderPassHandle, resourceHandle, resource_usage, InvalidResourceUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

ResourceUsageHandle Builder::read_resource_generic(RenderPassHandle    renderPassHandle,
                                                   ResourceUsageHandle inputUsageHandle,
                                                   GPUResourceUsage    resource_usage)
{
    const ResourceHandle resourceHandle = GetResourceUsage(m_Graph, inputUsageHandle).Resource;
    Assert(resourceHandle != InvalidResourceHandle, "Invalid resource handle");

    const ResourceUsageHandle resourceUsageHandle = create_resource_usage(
        UsageType::RenderPassInput, renderPassHandle, resourceHandle, resource_usage, inputUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

ResourceUsageHandle Builder::write_resource_generic(RenderPassHandle    renderPassHandle,
                                                    ResourceUsageHandle inputUsageHandle,
                                                    GPUResourceUsage    resource_usage)
{
    // FIXME do something about the texture usage
    const GPUResourceUsage    read_usage = {};
    const ResourceUsageHandle readUsageHandle = read_texture(renderPassHandle, inputUsageHandle, read_usage);

    const ResourceHandle resourceHandle = GetResourceUsage(m_Graph, inputUsageHandle).Resource;
    Assert(resourceHandle != InvalidResourceHandle, "Invalid resource handle");

    const ResourceUsageHandle resourceUsageHandle = create_resource_usage(
        UsageType::RenderPassOutput, renderPassHandle, resourceHandle, resource_usage, readUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

RenderPassHandle Builder::create_render_pass(const char* debug_name, bool hasSideEffects)
{
    // TODO test for name clashes
    RenderPass& newRenderPass = m_Graph.RenderPasses.emplace_back();
    newRenderPass.debug_name = debug_name;
    newRenderPass.HasSideEffects = hasSideEffects;

    return RenderPassHandle(m_Graph.RenderPasses.size() - 1);
}

ResourceUsageHandle Builder::create_texture(RenderPassHandle            renderPassHandle,
                                            const char*                 name,
                                            const GPUTextureProperties& texture_properties,
                                            GPUResourceUsage            texture_usage)
{
    return create_resource_generic(renderPassHandle, name, GPUResourceProperties{.texture = texture_properties},
                                   texture_usage, true);
}

ResourceUsageHandle Builder::create_buffer(RenderPassHandle           renderPassHandle,
                                           const char*                name,
                                           const GPUBufferProperties& buffer_properties,
                                           GPUResourceUsage           buffer_usage)
{
    return create_resource_generic(renderPassHandle, name, GPUResourceProperties{.buffer = buffer_properties},
                                   buffer_usage, false);
}

ResourceUsageHandle Builder::read_texture(RenderPassHandle    renderPassHandle,
                                          ResourceUsageHandle inputUsageHandle,
                                          GPUResourceUsage    texture_usage)
{
    return read_resource_generic(renderPassHandle, inputUsageHandle, texture_usage);
}

ResourceUsageHandle Builder::read_buffer(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                         GPUResourceUsage buffer_usage)
{
    return read_resource_generic(renderPassHandle, inputUsageHandle, buffer_usage);
}

ResourceUsageHandle Builder::write_texture(RenderPassHandle    renderPassHandle,
                                           ResourceUsageHandle inputUsageHandle,
                                           GPUResourceUsage    texture_usage)
{
    return write_resource_generic(renderPassHandle, inputUsageHandle, texture_usage);
}

ResourceUsageHandle Builder::write_buffer(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                          GPUResourceUsage buffer_usage)
{
    return write_resource_generic(renderPassHandle, inputUsageHandle, buffer_usage);
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

            Resource& resource = GetResource(frameGraph, resourceUsage);
            resource.IsUsed = true;
        }
    }
} // namespace

void Builder::build()
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
}
} // namespace Reaper::FrameGraph
