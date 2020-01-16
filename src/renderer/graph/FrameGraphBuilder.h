#pragma once

#include "FrameGraph.h"
#include "FrameGraphBasicTypes.h"

namespace Reaper
{
struct GPUTextureProperties;
// enum GPUTextureUsage;
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
    RenderPassHandle CreateRenderPass(const char* identifier, bool hasSideEffects = false);

public:
    ResourceUsageHandle CreateTexture(RenderPassHandle            renderPass,
                                      const char*                 name,
                                      const GPUTextureProperties& resourceDesc,
                                      TGPUTextureUsage            usage);

    ResourceUsageHandle
    ReadTexture(RenderPassHandle renderPass, ResourceUsageHandle inputUsageHandle, TGPUTextureUsage usage);

    ResourceUsageHandle
    WriteTexture(RenderPassHandle renderPass, ResourceUsageHandle inputUsageHandle, TGPUTextureUsage usage);

public:
    void Build();

private:
    ResourceHandle
    CreateResource(RenderPassHandle renderPassHandle, const char* identifier, const GPUTextureProperties& descriptor);

    ResourceUsageHandle CreateResourceUsage(UsageType           usageType,
                                            ResourceHandle      resourceHandle,
                                            TGPUTextureUsage    textureUsage,
                                            ResourceUsageHandle parentUsageHandle);

private:
    FrameGraph& m_Graph;
};
} // namespace Reaper::FrameGraph
