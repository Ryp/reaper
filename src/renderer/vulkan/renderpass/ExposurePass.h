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
struct ExposurePassResources
{
    struct Reduce
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;

        VkDescriptorSet descriptor_set;
    } reduce;

    struct ReduceTail
    {
        VkDescriptorSetLayout descriptor_set_layout;
        VkPipelineLayout      pipeline_layout;
        u32                   pipeline_index;

        VkDescriptorSet descriptor_set;
    } reduce_tail;
};

struct VulkanBackend;
struct PipelineFactory;

ExposurePassResources create_exposure_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory);
void                  destroy_exposure_pass_resources(VulkanBackend& backend, const ExposurePassResources& resources);

namespace FrameGraph
{
    class Builder;
    struct FrameGraph;
} // namespace FrameGraph

struct ExposureFrameGraphRecord
{
    struct Reduce
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle scene_hdr;
        FrameGraph::ResourceUsageHandle exposure_texture;
        FrameGraph::ResourceUsageHandle tail_counter;
    } reduce;

    struct ReduceTail
    {
        FrameGraph::RenderPassHandle    pass_handle;
        FrameGraph::ResourceUsageHandle exposure_texture;
        FrameGraph::ResourceUsageHandle average_exposure;
        FrameGraph::ResourceUsageHandle tail_counter;
        FrameGraph::ResourceUsageHandle exposure_texture_tail;
    } reduce_tail;
};

ExposureFrameGraphRecord create_exposure_pass_record(FrameGraph::Builder&            builder,
                                                     FrameGraph::ResourceUsageHandle scene_hdr_usage_handle,
                                                     VkExtent2D                      render_extent);

struct SamplerResources;
class DescriptorWriteHelper;
struct FrameGraphResources;

void update_exposure_pass_descriptor_set(const FrameGraph::FrameGraph&   frame_graph,
                                         const FrameGraphResources&      frame_graph_resources,
                                         const ExposureFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                         const ExposurePassResources& resources,
                                         const SamplerResources&      sampler_resources);

struct CommandBuffer;
struct FrameGraphHelper;

void record_exposure_command_buffer(const FrameGraphHelper& frame_graph_helper, const ExposureFrameGraphRecord& record,
                                    CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                    const ExposurePassResources& pass_resources);
} // namespace Reaper
