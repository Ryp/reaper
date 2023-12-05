////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"

#include <vulkan_loader/Vulkan.h>

#include "ShadowConstants.h"

#include <glm/vec2.hpp>
#include <span>

#include <vector>

namespace Reaper
{
struct ShadowMapResources
{
    u32                   pipeline_index;
    VkPipelineLayout      pipeline_layout;
    VkDescriptorSetLayout desc_set_layout;

    std::vector<VkDescriptorSet> descriptor_sets;
};

struct VulkanBackend;
struct PipelineFactory;

ShadowMapResources create_shadow_map_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory);
void               destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources);

namespace FrameGraph
{
    class FrameGraph;
    class Builder;
} // namespace FrameGraph

struct ShadowFrameGraphRecord
{
    FrameGraph::RenderPassHandle                 pass_handle;
    std::vector<FrameGraph::ResourceUsageHandle> shadow_maps;
    FrameGraph::ResourceUsageHandle              meshlet_counters;
    FrameGraph::ResourceUsageHandle              meshlet_indirect_draw_commands;
    FrameGraph::ResourceUsageHandle              meshlet_visible_index_buffer;
};

struct CullMeshletsFrameGraphRecord;
struct PreparedData;

ShadowFrameGraphRecord create_shadow_map_pass_record(FrameGraph::Builder&                builder,
                                                     const CullMeshletsFrameGraphRecord& meshlet_pass,
                                                     const PreparedData&                 prepared);

struct StorageBufferAllocator;
class DescriptorWriteHelper;

void update_shadow_map_resources(DescriptorWriteHelper& write_helper, StorageBufferAllocator& frame_storage_allocator,
                                 const PreparedData& prepared, ShadowMapResources& resources,
                                 GPUBuffer& vertex_position_buffer);

struct FrameGraphHelper;
struct CommandBuffer;

void record_shadow_map_command_buffer(const FrameGraphHelper&       frame_graph_helper,
                                      const ShadowFrameGraphRecord& pass_record, CommandBuffer& cmdBuffer,
                                      const PipelineFactory& pipeline_factory, const PreparedData& prepared,
                                      ShadowMapResources& resources);
} // namespace Reaper
