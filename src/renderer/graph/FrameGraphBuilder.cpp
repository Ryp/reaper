////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameGraphBuilder.h"

#include "GraphDebug.h"

#include <core/Assert.h>
#include <profiling/Scope.h>

#include <cstring>

namespace Reaper::FrameGraph
{
Builder::Builder(FrameGraph& frameGraph)
    : m_Graph(frameGraph)
{}

ResourceHandle Builder::create_resource(const char* debug_name, const GPUResourceProperties& properties,
                                        bool is_texture)
{
    auto& resource_array = is_texture ? m_Graph.TextureResources : m_Graph.BufferResources;

    // TODO test for name clashes
    Resource& newResource = resource_array.emplace_back();
    newResource.debug_name = debug_name;
    newResource.properties = properties;

    ResourceHandle new_handle;
    new_handle.index = resource_array.size() - 1;
    new_handle.is_texture = is_texture;

    return new_handle;
}

ResourceUsageHandle Builder::create_resource_usage(u32                 usageType,
                                                   RenderPassHandle    renderPassHandle,
                                                   ResourceHandle      resourceHandle,
                                                   GPUResourceUsage    resourceUsage,
                                                   ResourceUsageHandle parentUsageHandle)
{
    ResourceUsage& newResourceUsage = m_Graph.ResourceUsages.emplace_back();

    Assert(is_valid(resourceHandle), "Invalid resource handle");
    newResourceUsage.type = usageType;
    newResourceUsage.resource_handle = resourceHandle;
    newResourceUsage.render_pass = renderPassHandle;
    newResourceUsage.parent_usage_handle = parentUsageHandle;
    newResourceUsage.usage = resourceUsage;

    return ResourceUsageHandle(m_Graph.ResourceUsages.size() - 1);
}

ResourceUsageHandle Builder::create_resource_generic(RenderPassHandle renderPassHandle, const char* name,
                                                     const GPUResourceProperties& properties,
                                                     GPUResourceUsage resource_usage, bool is_texture)
{
    const ResourceHandle resourceHandle = create_resource(name, properties, is_texture);
    Assert(is_valid(resourceHandle), "Invalid resource handle");

    const ResourceUsageHandle resourceUsageHandle = create_resource_usage(
        UsageType::Output, renderPassHandle, resourceHandle, resource_usage, InvalidResourceUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

ResourceUsageHandle Builder::read_resource_generic(RenderPassHandle    renderPassHandle,
                                                   ResourceUsageHandle inputUsageHandle,
                                                   GPUResourceUsage    resource_usage)
{
    const ResourceHandle resourceHandle = GetResourceUsage(m_Graph, inputUsageHandle).resource_handle;
    Assert(is_valid(resourceHandle), "Invalid resource handle");

    const ResourceUsageHandle resourceUsageHandle =
        create_resource_usage(UsageType::Input, renderPassHandle, resourceHandle, resource_usage, inputUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

ResourceUsageHandle Builder::write_resource_generic(RenderPassHandle    renderPassHandle,
                                                    ResourceUsageHandle inputUsageHandle,
                                                    GPUResourceUsage    resource_usage)
{
    const ResourceHandle resourceHandle = GetResourceUsage(m_Graph, inputUsageHandle).resource_handle;
    Assert(is_valid(resourceHandle), "Invalid resource handle");

    const ResourceUsageHandle resourceUsageHandle = create_resource_usage(
        UsageType::Input | UsageType::Output, renderPassHandle, resourceHandle, resource_usage, inputUsageHandle);

    RenderPass& renderPass = m_Graph.RenderPasses[renderPassHandle];
    renderPass.ResourceUsageHandles.push_back(resourceUsageHandle);

    return resourceUsageHandle;
}

RenderPassHandle Builder::create_render_pass(const char* debug_name, bool hasSideEffects)
{
    // TODO test for name clashes
    RenderPass& newRenderPass = m_Graph.RenderPasses.emplace_back();
    newRenderPass.debug_name = debug_name;
    newRenderPass.has_side_effects = hasSideEffects;

    return RenderPassHandle(m_Graph.RenderPasses.size() - 1);
}

ResourceUsageHandle Builder::create_texture(RenderPassHandle            renderPassHandle,
                                            const char*                 name,
                                            const GPUTextureProperties& texture_properties,
                                            GPUResourceUsage            texture_usage)
{
    GPUResourceProperties resource_properties;
    resource_properties.texture = texture_properties;

    return create_resource_generic(renderPassHandle, name, resource_properties, texture_usage, true);
}

ResourceUsageHandle Builder::create_buffer(RenderPassHandle           renderPassHandle,
                                           const char*                name,
                                           const GPUBufferProperties& buffer_properties,
                                           GPUResourceUsage           buffer_usage)
{
    GPUResourceProperties resource_properties;
    resource_properties.buffer = buffer_properties;

    return create_resource_generic(renderPassHandle, name, resource_properties, buffer_usage, false);
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
    DirectedAcyclicGraph ConvertFrameGraphToDAG(const FrameGraph&                              frameGraph,
                                                std::vector<DirectedAcyclicGraph::index_type>& outRootNodes)
    {
        const u32 resourceUsageCount = frameGraph.ResourceUsages.size();
        const u32 renderPassCount = frameGraph.RenderPasses.size();

        // Resource usage node indexes are the same for both graphs and
        // can be used interchangeably.
        // Renderpass node indexes are NOT the same for both graphs but
        // can be retrieved with an offset.
        DirectedAcyclicGraph dag;
        dag.Nodes.resize(resourceUsageCount + renderPassCount);

        // Convert edges
        for (u32 renderPassIndex = 0; renderPassIndex < renderPassCount; renderPassIndex++)
        {
            const RenderPass&                      renderPass = frameGraph.RenderPasses[renderPassIndex];
            const DirectedAcyclicGraph::index_type renderPassIndexInDAG = renderPassIndex + resourceUsageCount;

            // Renderpasses that have side effects cannot be pruned
            // We use them as terminal nodes in the DAG so that orphan nodes
            // can properly be pruned later.
            if (renderPass.has_side_effects)
            {
                outRootNodes.push_back(renderPassIndexInDAG);
            }

            for (const auto& resourceUsageHandle : renderPass.ResourceUsageHandles)
            {
                // Index conversion
                const DirectedAcyclicGraph::index_type resourceUsageIndexInDAG = resourceUsageHandle;

                const ResourceUsage& resourceUsage = GetResourceUsage(frameGraph, resourceUsageHandle);

                // Edges are directed in reverse compared to the rendering flow.
                // This part is critical and not necessarily intuitive because of this. Draw a graph!
                if (resourceUsage.type == UsageType::Input)
                {
                    dag.Nodes[renderPassIndexInDAG].Children.push_back(resourceUsageIndexInDAG);

                    // Add extra edge to link to the previously written/created resource
                    Assert(is_valid(resourceUsage.parent_usage_handle), "Invalid resource usage handle");
                    DirectedAcyclicGraph::index_type parentUsageIndexInDAG = resourceUsage.parent_usage_handle;

                    dag.Nodes[resourceUsageIndexInDAG].Children.push_back(parentUsageIndexInDAG);
                }
                else if (resourceUsage.type == UsageType::Output)
                {
                    dag.Nodes[resourceUsageIndexInDAG].Children.push_back(renderPassIndexInDAG);
                }
                else if (resourceUsage.type == (UsageType::Input | UsageType::Output))
                {
                    dag.Nodes[resourceUsageIndexInDAG].Children.push_back(renderPassIndexInDAG);

                    // Add extra edge to link to the previously written/created resource
                    Assert(is_valid(resourceUsage.parent_usage_handle), "Invalid resource usage handle");
                    DirectedAcyclicGraph::index_type parentUsageIndexInDAG = resourceUsage.parent_usage_handle;

                    dag.Nodes[resourceUsageIndexInDAG].Children.push_back(parentUsageIndexInDAG);
                }
                else
                {
                    AssertUnreachable();
                }
            }
        }

        // Check if the whole graph is likely to be pruned
        Assert(!outRootNodes.empty(), "No render passes were marked with the has_side_effects flag.");

        return dag;
    }

    // Thibault S. (08/02/2018)
    // This one is relatively simple, we recover infos from the DAG
    // to fill is_used fields.
    void FillFrameGraphUsedNodes(nonstd::span<const DirectedAcyclicGraph::index_type> usedNodes, FrameGraph& frameGraph)
    {
        // Reset the is_used flag
        for (auto& resourceUsage : frameGraph.ResourceUsages)
            resourceUsage.is_used = false;

        for (auto& texture_resource : frameGraph.TextureResources)
            texture_resource.is_used = false;

        for (auto& buffer_resource : frameGraph.BufferResources)
            buffer_resource.is_used = false;

        for (auto& renderPass : frameGraph.RenderPasses)
            renderPass.is_used = false;

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
                resource.is_used = true;
            }
            else
            {
                RenderPass& renderPass = frameGraph.RenderPasses[nodeIndexInDAG - resourceCount];
                renderPass.is_used = true;
            }
        }

        for (const auto& renderPass : frameGraph.RenderPasses)
        {
            if (!renderPass.is_used)
                continue;

            for (auto resourceUsageHandle : renderPass.ResourceUsageHandles)
            {
                ResourceUsage& resourceUsage = frameGraph.ResourceUsages[resourceUsageHandle];

                if (has_mask(resourceUsage.type, UsageType::Output))
                {
                    resourceUsage.is_used = true;
                }
            }
        }

        for (auto& resourceUsage : frameGraph.ResourceUsages)
        {
            if (!resourceUsage.is_used)
                continue;

            Resource& resource = GetResource(frameGraph, resourceUsage);
            resource.is_used = true;
        }
    }
} // namespace

void Builder::build()
{
    REAPER_PROFILE_SCOPE_FUNC();

    // Use the current graph to build an alternate representation
    // for easier processing.
    std::vector<DirectedAcyclicGraph::index_type> rootNodes;

    DirectedAcyclicGraph dag = ConvertFrameGraphToDAG(m_Graph, rootNodes);

    Assert(!HasCycles(dag, rootNodes), "The framegraph DAG has cycles");

    // Compute the set of useful nodes with a flood fill from root nodes
    std::vector<DirectedAcyclicGraph::index_type> closure;
    ComputeTransitiveClosure(dag, rootNodes, closure);

    // Fill usage info from the DAG into the frame graph
    FillFrameGraphUsedNodes(closure, m_Graph);
}
} // namespace Reaper::FrameGraph
