////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/api/Vulkan.h"

#include <vector>

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;

struct CullPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

CullPipelineInfo create_cull_pipeline(ReaperRoot& root, VulkanBackend& backend);

struct CullPassResources
{
    VkDescriptorSet descriptor_set;
};

struct CullResources
{
    std::vector<CullPassResources> passes;

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
                                                            VkDescriptorPool descriptor_pool);

VkDescriptorSet create_culling_compact_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                       CullResources& cull_resources, VkDescriptorSetLayout layout,
                                                       VkDescriptorPool descriptor_pool);

struct PreparedData;

struct CullOptions
{
    bool freeze_culling;
    bool use_compacted_draw;
};

void culling_prepare_buffers(const CullOptions& options, VulkanBackend& backend, const PreparedData& prepared,
                             CullResources& resources);
} // namespace Reaper
