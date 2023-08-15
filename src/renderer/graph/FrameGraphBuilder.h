////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

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
                                       GPUTextureAccess            texture_usage);

    ResourceUsageHandle create_texture(RenderPassHandle            renderPass,
                                       const char*                 name,
                                       const GPUTextureProperties& texture_properties,
                                       GPUTextureAccess            texture_access,
                                       GPUTextureView              texture_view);

    ResourceUsageHandle create_buffer(RenderPassHandle           renderPass,
                                      const char*                name,
                                      const GPUBufferProperties& buffer_properties,
                                      GPUBufferAccess            buffer_access);

    ResourceUsageHandle create_buffer(RenderPassHandle           renderPass,
                                      const char*                name,
                                      const GPUBufferProperties& buffer_properties,
                                      GPUBufferAccess            buffer_access,
                                      GPUBufferView              buffer_view);

    ResourceUsageHandle read_texture(RenderPassHandle renderPass, ResourceUsageHandle inputUsageHandle,
                                     GPUTextureAccess texture_access);

    ResourceUsageHandle read_texture(RenderPassHandle renderPass, ResourceUsageHandle inputUsageHandle,
                                     GPUTextureAccess texture_access, GPUTextureView texture_view);

    ResourceUsageHandle read_buffer(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                    GPUBufferAccess buffer_access);

    ResourceUsageHandle read_buffer(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                    GPUBufferAccess buffer_access, GPUBufferView buffer_view);

    ResourceUsageHandle write_texture(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                      GPUTextureAccess texture_access);

    ResourceUsageHandle write_texture(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                      GPUTextureAccess texture_access, GPUTextureView texture_view);

    ResourceUsageHandle write_buffer(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                     GPUBufferAccess buffer_access);

    ResourceUsageHandle write_buffer(RenderPassHandle renderPassHandle, ResourceUsageHandle inputUsageHandle,
                                     GPUBufferAccess buffer_access, GPUBufferView buffer_view);

public:
    void build();

private:
    ResourceHandle create_resource(const char* debug_name, const GPUResourceProperties& properties, bool is_texture);

    ResourceUsageHandle create_resource_usage(u32                 usageType,
                                              RenderPassHandle    renderPassHandle,
                                              ResourceHandle      resourceHandle,
                                              GPUResourceAccess   resource_access,
                                              GPUResourceView     resource_view,
                                              ResourceUsageHandle parentUsageHandle);

    ResourceUsageHandle create_resource_generic(RenderPassHandle             renderPassHandle,
                                                const char*                  name,
                                                const GPUResourceProperties& properties,
                                                GPUResourceAccess            resource_access,
                                                GPUResourceView              resource_view,
                                                bool                         is_texture);

    ResourceUsageHandle read_resource_generic(RenderPassHandle     renderPassHandle,
                                              const ResourceUsage& inputUsage,
                                              ResourceUsageHandle  inputUsageHandle,
                                              GPUResourceAccess    resource_access,
                                              GPUResourceView      resource_view);

    ResourceUsageHandle write_resource_generic(RenderPassHandle     renderPassHandle,
                                               const ResourceUsage& inputUsage,
                                               ResourceUsageHandle  inputUsageHandle,
                                               GPUResourceAccess    resource_access,
                                               GPUResourceView      resource_view);

private:
    FrameGraph& m_Graph;
};
} // namespace Reaper::FrameGraph
