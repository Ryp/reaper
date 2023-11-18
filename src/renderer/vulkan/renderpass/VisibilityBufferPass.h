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
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout desc_set_layout;
};

struct VisibilityBufferPassResources
{
    VisibilityBufferPipelineInfo pipe;
    VkDescriptorSet              descriptor_set;

    VisibilityBufferPipelineInfo fill_pipe;
    VkDescriptorSet              descriptor_set_fill;
};

struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

VisibilityBufferPassResources create_vis_buffer_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                               const ShaderModules& shader_modules);
void destroy_vis_buffer_pass_resources(VulkanBackend& backend, VisibilityBufferPassResources& resources);

namespace FrameGraph
{
    class FrameGraph;
    class Builder;
} // namespace FrameGraph

struct VisBufferFrameGraphRecord
{
    GPUTextureProperties scene_depth_properties;

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
        FrameGraph::ResourceUsageHandle gbuffer_rt0;
        FrameGraph::ResourceUsageHandle gbuffer_rt1;
        FrameGraph::ResourceUsageHandle meshlet_visible_index_buffer;
        FrameGraph::ResourceUsageHandle visible_meshlet_buffer;
    } fill_gbuffer;
};

struct TiledLightingFrame;
struct GPUTextureProperties;
struct CullMeshletsFrameGraphRecord;

VisBufferFrameGraphRecord create_vis_buffer_pass_record(FrameGraph::Builder&                builder,
                                                        const CullMeshletsFrameGraphRecord& meshlet_pass,
                                                        VkExtent2D                          render_extent);

struct FrameGraphResources;
struct MaterialResources;
struct MeshCache;
struct SamplerResources;
class DescriptorWriteHelper;
struct PreparedData;
struct StorageBufferAllocator;

void update_vis_buffer_pass_resources(const FrameGraph::FrameGraph&        frame_graph,
                                      const FrameGraphResources&           frame_graph_resources,
                                      const VisBufferFrameGraphRecord&     record,
                                      DescriptorWriteHelper&               write_helper,
                                      StorageBufferAllocator&              frame_storage_allocator,
                                      const VisibilityBufferPassResources& resources,
                                      const PreparedData&                  prepared,
                                      const SamplerResources&              sampler_resources,
                                      const MaterialResources&             material_resources,
                                      const MeshCache&                     mesh_cache);

struct FrameGraphHelper;
struct CommandBuffer;

void record_vis_buffer_pass_command_buffer(const FrameGraphHelper&                  frame_graph_helper,
                                           const VisBufferFrameGraphRecord::Render& pass_record,
                                           CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                           const VisibilityBufferPassResources& pass_resources);

void record_fill_gbuffer_pass_command_buffer(const FrameGraphHelper&                       frame_graph_helper,
                                             const VisBufferFrameGraphRecord::FillGBuffer& pass_record,
                                             CommandBuffer&                                cmdBuffer,
                                             const VisibilityBufferPassResources&          resources,
                                             VkExtent2D                                    render_extent);
} // namespace Reaper
