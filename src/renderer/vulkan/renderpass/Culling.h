////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CullingConstants.h"

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/api/Vulkan.h"

#include <vector>

namespace Reaper
{
constexpr VkIndexType get_vk_culling_index_type()
{
    if constexpr (IndexSizeBytes == 2)
        return VK_INDEX_TYPE_UINT16;
    else
    {
        static_assert(IndexSizeBytes == 4, "Invalid index size");
        return VK_INDEX_TYPE_UINT32;
    }
}

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

    BufferInfo cullPassConstantBuffer;
    BufferInfo cullInstanceParamsBuffer;
    BufferInfo countersBuffer;
    BufferInfo dynamicMeshletBuffer;
    BufferInfo indirectDispatchBuffer;
    BufferInfo dynamicIndexBuffer;
    BufferInfo indirectDrawBuffer;
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

void record_culling_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared, CullResources& resources);

u32 get_indirect_draw_counter_offset(u32 pass_index);
} // namespace Reaper
