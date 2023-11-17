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
struct FrameGraphBuffer;
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
};

struct SamplerResources;
class DescriptorWriteHelper;
struct FrameGraphTexture;

void update_hzb_pass_descriptor_set(DescriptorWriteHelper& write_helper, const HZBPassResources& resources,
                                    const SamplerResources& sampler_resources, const FrameGraphTexture& scene_depth,
                                    const FrameGraphTexture& hzb_texture);

struct CommandBuffer;

void record_hzb_command_buffer(CommandBuffer& cmdBuffer, const HZBPassResources& pass_resources,
                               VkExtent2D depth_extent, VkExtent2D hzb_extent);

} // namespace Reaper
