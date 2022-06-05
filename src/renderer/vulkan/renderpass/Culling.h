////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include <vulkan_loader/Vulkan.h>

#include <vector>

namespace Reaper
{
// FIXME
struct SimplePipeline
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct CullResources
{
    SimplePipeline cullMeshletPipe;
    SimplePipeline cullMeshletPrepIndirect;
    SimplePipeline cullTrianglesPipe;

    std::vector<VkDescriptorSet> cull_meshlet_descriptor_sets;
    std::vector<VkDescriptorSet> cull_triangles_descriptor_sets;
    VkDescriptorSet              cull_prepare_descriptor_set;

    BufferInfo cullInstanceParamsBuffer;
    BufferInfo countersBuffer;
    BufferInfo countersBufferCPU;
    BufferInfo dynamicMeshletBuffer;
    BufferInfo indirectDispatchBuffer;
    BufferInfo dynamicIndexBuffer;
    BufferInfo indirectDrawBuffer;

    VkEvent countersReadyEvent;
};

struct ReaperRoot;
struct VulkanBackend;

CullResources create_culling_resources(ReaperRoot& root, VulkanBackend& backend);
void          destroy_culling_resources(VulkanBackend& backend, CullResources& resources);

struct PreparedData;

void upload_culling_resources(VulkanBackend& backend, const PreparedData& prepared, CullResources& resources);

struct MeshCache;

void update_culling_pass_descriptor_sets(VulkanBackend& backend, const PreparedData& prepared, CullResources& resources,
                                         const MeshCache& mesh_cache);

struct CommandBuffer;

void record_culling_command_buffer(ReaperRoot& root, CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                   CullResources& resources);

struct CullingStats
{
    u32 pass_index;
    u32 surviving_meshlet_count;
    u32 surviving_triangle_count;
    u32 indirect_draw_command_count;
};

std::vector<CullingStats> get_gpu_culling_stats(VulkanBackend& backend, const PreparedData& prepared,
                                                CullResources& resources);

struct CullingDrawParams
{
    u64         counter_buffer_offset;
    u64         index_buffer_offset;
    VkIndexType index_type;
    u64         command_buffer_offset;
    u32         command_buffer_max_count;
};

CullingDrawParams get_culling_draw_params(u32 pass_index);
} // namespace Reaper
