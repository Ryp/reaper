////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/ResourceHandle.h"
#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"

#include <vulkan_loader/Vulkan.h>

#include <glm/vec2.hpp>

#include <vector>

namespace Reaper
{
struct VisibilityBufferPipelineInfo
{
    u32                   pipeline_index;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout desc_set_layout;
};

struct VisibilityBufferPassResources
{
    VisibilityBufferPipelineInfo pipe;
    VisibilityBufferPipelineInfo pipe_msaa;
    VkDescriptorSet              descriptor_set;

    VisibilityBufferPipelineInfo fill_pipe;
    VisibilityBufferPipelineInfo fill_pipe_msaa;
    VisibilityBufferPipelineInfo fill_pipe_msaa_with_resolve;
    VkDescriptorSet              descriptor_set_fill;

    VisibilityBufferPipelineInfo legacy_resolve_pipe;
    VkDescriptorSet              descriptor_set_legacy_resolve;
};

struct VulkanBackend;
struct PipelineFactory;

VisibilityBufferPassResources create_vis_buffer_pass_resources(VulkanBackend&   backend,
                                                               PipelineFactory& pipeline_factory);
void destroy_vis_buffer_pass_resources(VulkanBackend& backend, VisibilityBufferPassResources& resources);

namespace FrameGraph
{
    struct FrameGraph;
    class Builder;
} // namespace FrameGraph

struct VisBufferFrameGraphRecord
{
    struct Render
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle vis_buffer;
        FrameGraph::ResourceUsageHandle depth;
        FrameGraph::ResourceUsageHandle meshlet_counters;
        FrameGraph::ResourceUsageHandle meshlet_indirect_draw_commands;
        FrameGraph::ResourceUsageHandle meshlet_visible_index_buffer;
        FrameGraph::ResourceUsageHandle visible_meshlet_buffer;
    } render;

    struct FillGBuffer
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle vis_buffer;
        FrameGraph::ResourceUsageHandle vis_buffer_depth_msaa;
        FrameGraph::ResourceUsageHandle resolved_depth;
        FrameGraph::ResourceUsageHandle gbuffer_rt0;
        FrameGraph::ResourceUsageHandle gbuffer_rt1;
        FrameGraph::ResourceUsageHandle meshlet_visible_index_buffer;
        FrameGraph::ResourceUsageHandle visible_meshlet_buffer;
    } fill_gbuffer;

    // Used when MSAA is on and there's no support for shader stores to depth
    struct LegacyDepthResolve
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle vis_buffer_depth_msaa;
        FrameGraph::ResourceUsageHandle resolved_depth;
    } legacy_depth_resolve;

    // NOTE: dynamically set to whoever produces the usable depth for further render passes
    FrameGraph::ResourceUsageHandle depth;

    GPUTextureProperties scene_depth_properties;
};

struct TiledLightingFrame;
struct GPUTextureProperties;
struct CullMeshletsFrameGraphRecord;

VisBufferFrameGraphRecord create_vis_buffer_pass_record(FrameGraph::Builder&                builder,
                                                        const CullMeshletsFrameGraphRecord& meshlet_pass,
                                                        VkExtent2D render_extent, bool enable_msaa,
                                                        bool support_shader_stores_to_depth);

struct FrameGraphResources;
struct MaterialResources;
struct MeshCache;
struct SamplerResources;
class DescriptorWriteHelper;
struct PreparedData;
struct StorageBufferAllocator;

void update_vis_buffer_pass_resources(const FrameGraph::FrameGraph&    frame_graph,
                                      const FrameGraphResources&       frame_graph_resources,
                                      const VisBufferFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                      StorageBufferAllocator&              frame_storage_allocator,
                                      const VisibilityBufferPassResources& resources, const PreparedData& prepared,
                                      const SamplerResources&  sampler_resources,
                                      const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                      bool enable_msaa, bool support_shader_stores_to_depth);

struct FrameGraphHelper;
struct CommandBuffer;

void record_vis_buffer_pass_command_buffer(const FrameGraphHelper&                  frame_graph_helper,
                                           const VisBufferFrameGraphRecord::Render& pass_record,
                                           CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                           const PreparedData&                  prepared,
                                           const VisibilityBufferPassResources& pass_resources, bool enable_msaa);

void record_fill_gbuffer_pass_command_buffer(const FrameGraphHelper&                       frame_graph_helper,
                                             const VisBufferFrameGraphRecord::FillGBuffer& pass_record,
                                             CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                             const VisibilityBufferPassResources& resources, VkExtent2D render_extent,
                                             bool enable_msaa, bool support_shader_stores_to_depth);

void record_legacy_depth_resolve_pass_command_buffer(const FrameGraphHelper& frame_graph_helper,
                                                     const VisBufferFrameGraphRecord::LegacyDepthResolve& pass_record,
                                                     CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                                     const VisibilityBufferPassResources& resources, bool enable_msaa,
                                                     bool support_shader_stores_to_depth);
} // namespace Reaper
