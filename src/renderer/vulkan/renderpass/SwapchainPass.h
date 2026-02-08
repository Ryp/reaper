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
struct SwapchainPassResources
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;

    VkDescriptorSet descriptor_set;
};

struct VulkanBackend;
struct ShaderModules;

SwapchainPassResources create_swapchain_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules);
void destroy_swapchain_pass_resources(VulkanBackend& backend, const SwapchainPassResources& resources);

void reload_swapchain_pipeline(VulkanBackend& backend, const ShaderModules& shader_modules,
                               SwapchainPassResources& resources);

namespace FrameGraph
{
    class Builder;
    struct FrameGraph;
} // namespace FrameGraph

struct SwapchainFrameGraphRecord
{
    FrameGraph::RenderPassHandle    pass_handle;
    FrameGraph::ResourceUsageHandle scene_hdr;
    FrameGraph::ResourceUsageHandle lighting_result;
    FrameGraph::ResourceUsageHandle gui;
    FrameGraph::ResourceUsageHandle histogram; // FIXME unused for now
    FrameGraph::ResourceUsageHandle average_exposure;
    FrameGraph::ResourceUsageHandle tile_debug;
};

SwapchainFrameGraphRecord
create_swapchain_pass_record(FrameGraph::Builder&            builder,
                             FrameGraph::ResourceUsageHandle scene_hdr_usage_handle,
                             FrameGraph::ResourceUsageHandle split_tiled_lighting_hdr_usage_handle,
                             FrameGraph::ResourceUsageHandle gui_sdr_usage_handle,
                             FrameGraph::ResourceUsageHandle histogram_buffer_usage_handle,
                             FrameGraph::ResourceUsageHandle average_exposure_usage_handle,
                             FrameGraph::ResourceUsageHandle tiled_debug_texture_overlay_usage_handle);

struct FrameGraphResources;
class DescriptorWriteHelper;
struct SamplerResources;

void update_swapchain_pass_descriptor_set(const FrameGraph::FrameGraph&    frame_graph,
                                          const FrameGraphResources&       frame_graph_resources,
                                          const SwapchainFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                          const SwapchainPassResources& resources,
                                          const SamplerResources&       sampler_resources);

struct CommandBuffer;
struct FrameGraphHelper;

void record_swapchain_command_buffer(const FrameGraphHelper&          frame_graph_helper,
                                     const SwapchainFrameGraphRecord& pass_record, CommandBuffer& cmdBuffer,
                                     const SwapchainPassResources& pass_resources, VkImageView swapchain_buffer_view,
                                     VkExtent2D swapchain_extent, float exposure_compensation_stops,
                                     float tonemap_min_nits, float tonemap_max_nits, float sdr_ui_max_brightness_nits,
                                     float sdr_peak_brightness_nits);

} // namespace Reaper
