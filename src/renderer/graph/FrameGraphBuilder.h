////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "FrameGraph.h"
#include "FrameGraphBasicTypes.h"

#include <nonstd/span.hpp>

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
    RenderPassHandle create_render_pass(const char* debug_name, bool has_side_effects = false);

public:
    ResourceUsageHandle
    create_texture(RenderPassHandle                   render_pass_handle,
                   const char*                        name,
                   const GPUTextureProperties&        texture_properties,
                   GPUTextureAccess                   texture_access,
                   nonstd::span<const GPUTextureView> additional_texture_views = nonstd::span<const GPUTextureView>());

    ResourceUsageHandle
    create_buffer(RenderPassHandle                  render_pass_handle,
                  const char*                       name,
                  const GPUBufferProperties&        buffer_properties,
                  GPUBufferAccess                   buffer_access,
                  nonstd::span<const GPUBufferView> additional_buffer_views = nonstd::span<const GPUBufferView>());

    ResourceUsageHandle
    read_texture(RenderPassHandle render_pass_handle, ResourceUsageHandle input_usage_handle,
                 GPUTextureAccess                   texture_access,
                 nonstd::span<const GPUTextureView> additional_texture_views = nonstd::span<const GPUTextureView>());

    ResourceUsageHandle
    read_buffer(RenderPassHandle render_pass_handle, ResourceUsageHandle input_usage_handle,
                GPUBufferAccess                   buffer_access,
                nonstd::span<const GPUBufferView> additional_buffer_views = nonstd::span<const GPUBufferView>());

    ResourceUsageHandle
    write_texture(RenderPassHandle render_pass_handle, ResourceUsageHandle input_usage_handle,
                  GPUTextureAccess                   texture_access,
                  nonstd::span<const GPUTextureView> additional_texture_views = nonstd::span<const GPUTextureView>());

    ResourceUsageHandle
    write_buffer(RenderPassHandle render_pass_handle, ResourceUsageHandle input_usage_handle,
                 GPUBufferAccess                   buffer_access,
                 nonstd::span<const GPUBufferView> additional_buffer_views = nonstd::span<const GPUBufferView>());

public:
    void build();

private:
    ResourceViewHandles allocate_texture_views(nonstd::span<const GPUTextureView> texture_views);
    ResourceViewHandles allocate_buffer_views(nonstd::span<const GPUBufferView> buffer_views);

    ResourceHandle create_resource(const char* debug_name, const GPUResourceProperties& properties, bool is_texture);

    ResourceUsageHandle create_resource_usage(u32                        usage_type,
                                              RenderPassHandle           render_pass_handle,
                                              ResourceHandle             resource_handle,
                                              GPUResourceAccess          resource_access,
                                              ResourceUsageHandle        parentUsageHandle,
                                              const ResourceViewHandles& view_handles);

    ResourceUsageHandle create_resource_generic(RenderPassHandle             render_pass_handle,
                                                const char*                  name,
                                                const GPUResourceProperties& properties,
                                                GPUResourceAccess            resource_access,
                                                bool                         is_texture,
                                                const ResourceViewHandles&   view_handles);

    ResourceUsageHandle read_resource_generic(RenderPassHandle           render_pass_handle,
                                              ResourceUsageHandle        input_usage_handle,
                                              GPUResourceAccess          resource_access,
                                              const ResourceViewHandles& view_handles);

    ResourceUsageHandle write_resource_generic(RenderPassHandle           render_pass_handle,
                                               ResourceUsageHandle        input_usage_handle,
                                               GPUResourceAccess          resource_access,
                                               const ResourceViewHandles& view_handles);

private:
    FrameGraph& m_Graph;
};
} // namespace Reaper::FrameGraph
