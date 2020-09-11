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

CullPipelineInfo create_cull_pipeline(ReaperRoot& root, VulkanBackend& backend);

struct CompactionPrepPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

CompactionPrepPipelineInfo create_compaction_prep_pipeline(ReaperRoot& root, VulkanBackend& backend);

struct CompactionPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

CompactionPipelineInfo create_compaction_pipeline(ReaperRoot& root, VulkanBackend& backend);

struct CullPassResources
{
    VkDescriptorSet cull_descriptor_set;
    VkDescriptorSet compact_prep_descriptor_set;
    VkDescriptorSet compact_descriptor_set;
};

struct CullResources
{
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

VkDescriptorSet create_culling_descriptor_sets(ReaperRoot& root, VulkanBackend& backend, CullResources& cull_resources,
                                               VkDescriptorSetLayout layout, VkDescriptorPool descriptor_pool,
                                               BufferInfo& staticIndexBuffer, BufferInfo& vertexBufferPosition,
                                               u32 pass_index);

VkDescriptorSet create_culling_compact_prep_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                            CullResources& cull_resources, VkDescriptorSetLayout layout,
                                                            VkDescriptorPool descriptor_pool, u32 pass_index);

VkDescriptorSet create_culling_compact_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                       CullResources& cull_resources, VkDescriptorSetLayout layout,
                                                       VkDescriptorPool descriptor_pool, u32 pass_index);

struct PreparedData;

struct CullOptions
{
    bool freeze_culling;
    bool use_compacted_draw;
};

void culling_prepare_buffers(const CullOptions& options, VulkanBackend& backend, const PreparedData& prepared,
                             CullResources& resources);
} // namespace Reaper
