////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"

#include <span>

namespace Reaper
{
struct TiledLightingPassResources
{
    VkDescriptorSetLayout tiled_lighting_descriptor_set_layout;
    VkPipelineLayout      tiled_lighting_pipeline_layout;
    VkPipeline            tiled_lighting_pipeline;

    VkDescriptorSet tiled_lighting_descriptor_set;

    GPUBuffer tiled_lighting_constant_buffer;

    // Debug
    VkDescriptorSetLayout tiled_lighting_debug_descriptor_set_layout;
    VkPipelineLayout      tiled_lighting_debug_pipeline_layout;
    VkPipeline            tiled_lighting_debug_pipeline;

    VkDescriptorSet tiled_lighting_debug_descriptor_set;
};

struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

TiledLightingPassResources create_tiled_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                                const ShaderModules& shader_modules);
void destroy_tiled_lighting_pass_resources(VulkanBackend& backend, TiledLightingPassResources& resources);

struct SamplerResources;
class DescriptorWriteHelper;
struct FrameGraphTexture;
struct FrameGraphBuffer;
struct LightingPassResources;
struct MaterialResources;

void update_tiled_lighting_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                                const LightingPassResources&      lighting_resources,
                                                const TiledLightingPassResources& resources,
                                                const SamplerResources&           sampler_resources,
                                                const FrameGraphBuffer&           light_list_buffer,
                                                const FrameGraphTexture&          gbuffer_rt0,
                                                const FrameGraphTexture&          gbuffer_rt1,
                                                const FrameGraphTexture&          main_view_depth,
                                                const FrameGraphTexture&          lighting_output,
                                                const FrameGraphBuffer&           tile_debug_buffer,
                                                std::span<const FrameGraphTexture>
                                                    shadow_maps);

struct PreparedData;

void upload_tiled_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                                TiledLightingPassResources& resources);

struct CommandBuffer;

void record_tiled_lighting_command_buffer(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                                          VkExtent2D render_extent, VkExtent2D tile_extent);

// Debug
void update_tiled_lighting_debug_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                                      const TiledLightingPassResources& resources,
                                                      const FrameGraphBuffer&           tile_debug_buffer,
                                                      const FrameGraphTexture&          tile_debug_texture);

void record_tiled_lighting_debug_command_buffer(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                                                VkExtent2D render_extent, VkExtent2D tile_extent);
} // namespace Reaper
