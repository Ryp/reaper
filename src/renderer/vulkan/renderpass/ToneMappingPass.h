////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2024 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"

#include <vulkan_loader/Vulkan.h>

namespace Reaper
{
struct ToneMapPassResources
{
    u32                   pipeline_index;
    VkPipelineLayout      pipeline_layout;
    VkDescriptorSetLayout descriptor_set_layout;

    VkDescriptorSet descriptor_set;
};

struct VulkanBackend;
struct PipelineFactory;

ToneMapPassResources create_tone_map_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory);
void                 destroy_tone_map_pass_resources(VulkanBackend& backend, const ToneMapPassResources& resources);

namespace FrameGraph
{
    class Builder;
    struct FrameGraph;
} // namespace FrameGraph

struct ToneMapPassRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle tone_map_lut;
};

ToneMapPassRecord create_tone_map_pass_record(FrameGraph::Builder& builder);

struct FrameGraphResources;
class DescriptorWriteHelper;
struct SamplerResources;

void update_tone_map_pass_descriptor_set(const FrameGraph::FrameGraph& frame_graph,
                                         const FrameGraphResources&    frame_graph_resources,
                                         const ToneMapPassRecord&      record,
                                         DescriptorWriteHelper&        write_helper,
                                         const ToneMapPassResources&   resources);

struct CommandBuffer;
struct FrameGraphHelper;

void record_tone_map_command_buffer(const FrameGraphHelper& frame_graph_helper, const ToneMapPassRecord& pass_record,
                                    CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                    const ToneMapPassResources& pass_resources, float tonemap_min_nits,
                                    float tonemap_max_nits);
} // namespace Reaper
