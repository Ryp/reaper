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

// Thibault S. (28/07/2017) Retained mode API frame graph builder
//
// Takes ownership of a Framegraph at construction, you can use it again
// after it is built.
//
// NOTES:
// Only textures are supported for now. The API for 'buffers' should
// be very similar, maybe even simpler though.
//
class REAPER_RENDERER_API FrameGraphBuilder
{
public:
    FrameGraphBuilder(FrameGraph& frameGraph);

public:
    RenderPassHandle create_render_pass(const char* identifier, bool hasSideEffects = false);

public:
    ResourceUsageHandle create_texture(RenderPassHandle            renderPass,
                                       const char*                 name,
                                       const GPUTextureProperties& resourceDesc,
                                       GPUResourceUsage            usage);

    ResourceUsageHandle
    read_texture(RenderPassHandle renderPass, ResourceUsageHandle inputUsageHandle, GPUResourceUsage usage);

    ResourceUsageHandle
    WriteTexture(RenderPassHandle renderPass, ResourceUsageHandle inputUsageHandle, GPUResourceUsage usage);

public:
    void build();

private:
    ResourceHandle
    CreateResource(RenderPassHandle renderPassHandle, const char* identifier, const GPUTextureProperties& descriptor);

    ResourceUsageHandle CreateResourceUsage(UsageType           usageType,
                                            RenderPassHandle    renderPassHandle,
                                            ResourceHandle      resourceHandle,
                                            GPUResourceUsage    textureUsage,
                                            ResourceUsageHandle parentUsageHandle);

private:
    FrameGraph& m_Graph;
};
} // namespace Reaper::FrameGraph
