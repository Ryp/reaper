////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/ResourceHandle.h"
#include "renderer/graph/FrameGraphBasicTypes.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"

#include <vulkan_loader/Vulkan.h>

#include <glm/vec2.hpp>

#include <span>

#include <vector>

namespace Reaper
{
struct ForwardPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSetLayout desc_set_layout_material;
};

struct ForwardPassResources
{
    GPUBuffer pass_constant_buffer;
    GPUBuffer instance_buffer;

    ForwardPipelineInfo pipe;

    VkDescriptorSet descriptor_set;
    VkDescriptorSet material_descriptor_set;
};

struct VulkanBackend;
struct ShaderModules;

ForwardPassResources create_forward_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules);
void                 destroy_forward_pass_resources(VulkanBackend& backend, ForwardPassResources& resources);

namespace FrameGraph
{
    class FrameGraph;
}

struct ForwardFrameGraphRecord
{
    FrameGraph::RenderPassHandle                 pass_handle;
    FrameGraph::ResourceUsageHandle              scene_hdr;
    FrameGraph::ResourceUsageHandle              depth;
    std::vector<FrameGraph::ResourceUsageHandle> shadow_maps;
    FrameGraph::ResourceUsageHandle              meshlet_counters;
    FrameGraph::ResourceUsageHandle              meshlet_indirect_draw_commands;
    FrameGraph::ResourceUsageHandle              meshlet_visible_index_buffer;
    FrameGraph::ResourceUsageHandle              visible_meshlet_buffer;
};

struct MaterialResources;
struct MeshCache;
struct LightingPassResources;
struct SamplerResources;
class DescriptorWriteHelper;
struct FrameGraphBuffer;
struct FrameGraphTexture;

void update_forward_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const ForwardPassResources& resources,
                                         const FrameGraphBuffer&  visible_meshlet_buffer,
                                         const SamplerResources&  sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                         const LightingPassResources&       lighting_resources,
                                         std::span<const FrameGraphTexture> shadow_maps);

struct PreparedData;

void upload_forward_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                         ForwardPassResources& pass_resources);

struct CommandBuffer;

void record_forward_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                        const ForwardPassResources& pass_resources,
                                        const FrameGraphBuffer&     meshlet_counters,
                                        const FrameGraphBuffer&     meshlet_indirect_draw_commands,
                                        const FrameGraphBuffer&     meshlet_visible_index_buffer,
                                        const FrameGraphTexture& hdr_buffer, const FrameGraphTexture& depth_buffer);
} // namespace Reaper
