////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"
#include <vulkan_loader/Vulkan.h>

#include <glm/vec2.hpp>

#include <span>

#include <vector>

namespace Reaper
{
struct HZBPipelineInfo
{
    VkPipeline            handle;
    VkPipelineLayout      layout;
    VkDescriptorSetLayout desc_set_layout;
};

struct HZBPassResources
{
    HZBPipelineInfo hzb_pipe;

    VkDescriptorSet descriptor_set;
};

struct VulkanBackend;
struct ShaderModules;

HZBPassResources create_hzb_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules);
void             destroy_hzb_pass_resources(VulkanBackend& backend, const HZBPassResources& resources);

namespace FrameGraph
{
    class Builder;
    class FrameGraph;
} // namespace FrameGraph

struct HZBReduceFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle depth;
    FrameGraph::ResourceUsageHandle hzb_texture;
    GPUTextureProperties            hzb_properties;
};

struct TiledLightingFrame;

HZBReduceFrameGraphRecord create_hzb_pass_record(FrameGraph::Builder&            builder,
                                                 FrameGraph::ResourceUsageHandle depth_buffer_usage_handle,
                                                 const TiledLightingFrame&       tiled_lighting_frame);

struct SamplerResources;
class DescriptorWriteHelper;
struct FrameGraphResources;

void update_hzb_pass_descriptor_set(const FrameGraph::FrameGraph&    frame_graph,
                                    const FrameGraphResources&       frame_graph_resources,
                                    const HZBReduceFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                    const HZBPassResources& resources, const SamplerResources& sampler_resources);

struct FrameGraphHelper;
struct CommandBuffer;

void record_hzb_command_buffer(const FrameGraphHelper& frame_graph_helper, const HZBReduceFrameGraphRecord& pass_record,
                               CommandBuffer& cmdBuffer, const HZBPassResources& pass_resources,
                               VkExtent2D depth_extent, VkExtent2D hzb_extent);

} // namespace Reaper
