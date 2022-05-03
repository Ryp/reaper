#pragma once

#include "FrameGraph.h"
#include "FrameGraphBasicTypes.h"

namespace Reaper
{
struct GPUTextureProperties;
} // namespace Reaper

namespace Reaper::FrameGraph
{
struct FrameGraph;

// Retained mode API frame graph builder
//
// Takes ownership of a Framegraph at construction, you can use it again
// after it is built.
class REAPER_RENDERER_API Builder
{
public:
    Builder(FrameGraph& frameGraph);

public:
    RenderPassHandle create_render_pass(const char* debug_name, bool hasSideEffects = false);

public:
    ResourceUsageHandle create_texture(RenderPassHandle            renderPass,
                                       const char*                 name,
                                       const GPUTextureProperties& texture_properties,
                                       GPUResourceUsage            texture_usage);

    ResourceUsageHandle create_buffer(RenderPassHandle           renderPass,
                                      const char*                name,
                                      const GPUBufferProperties& buffer_properties,
                                      GPUResourceUsage           buffer_usage);

    ResourceUsageHandle read_texture(RenderPassHandle renderPass, ResourceUsageHandle inputUsageHandle,
                                     GPUResourceUsage texture_usage);

    ResourceUsageHandle read_buffer(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                    GPUResourceUsage buffer_usage);

    ResourceUsageHandle write_texture(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                      GPUResourceUsage texture_usage);

    ResourceUsageHandle write_buffer(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                     GPUResourceUsage buffer_usage);

public:
    void build();

private:
    ResourceHandle create_resource(const char* debug_name, const GPUResourceProperties& properties, bool is_texture);

    ResourceUsageHandle create_resource_usage(UsageType           usageType,
                                              RenderPassHandle    renderPassHandle,
                                              ResourceHandle      resourceHandle,
                                              GPUResourceUsage    resourceUsage,
                                              ResourceUsageHandle parentUsageHandle);

    ResourceUsageHandle create_resource_generic(RenderPassHandle renderPassHandle, const char* name,
                                                const GPUResourceProperties& properties, GPUResourceUsage usage,
                                                bool is_texture);

    ResourceUsageHandle read_resource_generic(RenderPassHandle    renderPassHandle,
                                              ResourceUsageHandle inputUsageHandle,
                                              GPUResourceUsage    usage);

    ResourceUsageHandle write_resource_generic(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                               GPUResourceUsage usage);

private:
    FrameGraph& m_Graph;
};
} // namespace Reaper::FrameGraph
