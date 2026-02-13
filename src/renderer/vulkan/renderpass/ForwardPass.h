////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/vulkan/Buffer.h"

#include <vulkan_loader/Vulkan.h>

#include <glm/vec2.hpp>

#include <vector>

namespace Reaper
{
struct ForwardPipelineInfo
{
    u32                   pipeline_index;
    VkPipelineLayout      pipeline_layout;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSetLayout desc_set_layout_material;
};

struct ForwardPassResources
{
    GPUBuffer pass_constant_buffer;
    GPUBuffer instance_buffer;

    ForwardPipelineInfo pipe;

    VkDescriptorSet descriptor_set;
    VkDescriptorSet material_descriptor_set;
};

struct VulkanBackend;
struct PipelineFactory;

ForwardPassResources create_forward_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory);
void                 destroy_forward_pass_resources(VulkanBackend& backend, ForwardPassResources& resources);

namespace FrameGraph
{
    class Builder;
    struct FrameGraph;
} // namespace FrameGraph

struct ForwardFrameGraphRecord
{
    FrameGraph::RenderPassHandle                 pass_handle;
    FrameGraph::ResourceUsageHandle              scene_hdr;
    FrameGraph::ResourceUsageHandle              depth;
    std::vector<FrameGraph::ResourceUsageHandle> shadow_maps;
    FrameGraph::ResourceUsageHandle              meshlet_counters;
    FrameGraph::ResourceUsageHandle              meshlet_indirect_draw_commands;
    FrameGraph::ResourceUsageHandle              meshlet_visible_index_buffer;
    FrameGraph::ResourceUsageHandle              visible_meshlet_buffer;
};

struct CullMeshletsFrameGraphRecord;
struct ShadowFrameGraphRecord;

ForwardFrameGraphRecord create_forward_pass_record(FrameGraph::Builder&                builder,
                                                   const CullMeshletsFrameGraphRecord& meshlet_pass,
                                                   const ShadowFrameGraphRecord&       shadow,
                                                   FrameGraph::ResourceUsageHandle     depth_buffer_usage_handle,
                                                   VkExtent2D                          render_extent);

struct FrameGraphResources;
struct StorageBufferAllocator;
struct MaterialResources;
struct MeshCache;
struct LightingPassResources;
struct PreparedData;
class DescriptorWriteHelper;
struct SamplerResources;

void update_forward_pass_descriptor_sets(VulkanBackend& backend, const FrameGraph::FrameGraph& frame_graph,
                                         const FrameGraphResources&     frame_graph_resources,
                                         const ForwardFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                         StorageBufferAllocator& frame_storage_allocator, const PreparedData& prepared,
                                         const ForwardPassResources& resources,
                                         const SamplerResources&     sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                         const LightingPassResources& lighting_resources);

struct CommandBuffer;
struct FrameGraphHelper;
struct PipelineFactory;

void record_forward_pass_command_buffer(const FrameGraphHelper&        frame_graph_helper,
                                        const ForwardFrameGraphRecord& pass_record, CommandBuffer& cmdBuffer,
                                        const PipelineFactory& pipeline_factory, const PreparedData& prepared,
                                        const ForwardPassResources& pass_resources);
} // namespace Reaper
