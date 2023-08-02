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

struct MeshletCullingResources
{
    SimplePipeline cull_meshlets_pipe;
    SimplePipeline cull_meshlets_prep_indirect_pipe;
    SimplePipeline cull_triangles_pipe;

    std::vector<VkDescriptorSet> cull_meshlet_descriptor_sets;
    std::vector<VkDescriptorSet> cull_triangles_descriptor_sets;
    VkDescriptorSet              cull_prepare_descriptor_set;

    BufferInfo mesh_instance_buffer;
    BufferInfo counters_buffer;
    BufferInfo counters_cpu_buffer;
    BufferInfo visible_meshlet_offsets_buffer;
    BufferInfo triangle_culling_indirect_dispatch_buffer;
    BufferInfo visible_index_buffer;
    BufferInfo visible_indirect_draw_commands_buffer;
    BufferInfo visible_meshlet_buffer;

    VkEvent countersReadyEvent;
};

struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

MeshletCullingResources create_meshlet_culling_resources(ReaperRoot& root, VulkanBackend& backend,
                                                         const ShaderModules& shader_modules);
void                    destroy_meshlet_culling_resources(VulkanBackend& backend, MeshletCullingResources& resources);

struct PreparedData;

void upload_meshlet_culling_resources(VulkanBackend& backend, const PreparedData& prepared,
                                      MeshletCullingResources& resources);

struct MeshCache;
struct DescriptorWriteHelper;

void update_meshlet_culling_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                                 MeshletCullingResources& resources, const MeshCache& mesh_cache);

struct CommandBuffer;

void record_meshlet_culling_command_buffer(ReaperRoot& root, CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                           MeshletCullingResources& resources);

struct MeshletCullingStats
{
    u32 pass_index;
    u32 surviving_meshlet_count;
    u32 surviving_triangle_count;
    u32 indirect_draw_command_count;
};

std::vector<MeshletCullingStats> get_meshlet_culling_gpu_stats(VulkanBackend& backend, const PreparedData& prepared,
                                                               MeshletCullingResources& resources);

struct MeshletDrawParams
{
    u64         counter_buffer_offset;
    u64         index_buffer_offset;
    VkIndexType index_type;
    u64         command_buffer_offset;
    u32         command_buffer_max_count;
};

MeshletDrawParams get_meshlet_draw_params(u32 pass_index);
BufferSubresource get_meshlet_visible_index_buffer_pass(u32 pass_index);
} // namespace Reaper
