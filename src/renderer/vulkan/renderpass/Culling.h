////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
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

struct ReaperRoot;
struct VulkanBackend;

struct CullPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct CompactionPrepPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct CompactionPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct CullPassResources
{
    VkDescriptorSet cull_descriptor_set;
    VkDescriptorSet compact_prep_descriptor_set;
    VkDescriptorSet compact_descriptor_set;
};

struct CullResources
{
    CullPipelineInfo           cullPipe;
    CompactionPrepPipelineInfo compactPrepPipe;
    CompactionPipelineInfo     compactionPipe;

    // These are valid for ONE FRAME ONLY
    std::vector<CullPassResources> passes;

    BufferInfo cullPassConstantBuffer;
    BufferInfo cullInstanceParamsBuffer;
    BufferInfo indirectDrawCountBuffer;
    BufferInfo dynamicIndexBuffer;
    BufferInfo indirectDrawBuffer;
    BufferInfo compactIndirectDrawBuffer;
    BufferInfo compactIndirectDrawCountBuffer;
    BufferInfo compactionIndirectDispatchBuffer;
};

CullResources create_culling_resources(ReaperRoot& root, VulkanBackend& backend);
void          destroy_culling_resources(VulkanBackend& backend, CullResources& resources);

CullPassResources create_culling_pass_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                      CullResources& resources, u32 pass_index,
                                                      const BufferInfo& indexBuffer,
                                                      const BufferInfo& vertexBufferPosition);

struct PreparedData;

struct CullOptions
{
    bool freeze_culling;
    bool use_compacted_draw;
};

struct MeshCache;

void prepare_culling_resources(ReaperRoot& root, const CullOptions& options, VulkanBackend& backend,
                               const PreparedData& prepared, CullResources& resources, const MeshCache& mesh_cache);

void record_culling_command_buffer(const CullOptions& options, VkCommandBuffer cmdBuffer, const PreparedData& prepared,
                                   CullResources& resources);
} // namespace Reaper
