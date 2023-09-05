////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"

#include <vector>

namespace Reaper
{
static constexpr u32 DebugGeometryCountMax = 2048;

struct DebugMeshAlloc
{
    u32 index_offset;
    u32 index_count;
    u32 vertex_offset;
    u32 vertex_count;
};

struct DebugGeometryPassResources
{
    VkDescriptorSetLayout build_cmds_descriptor_set_layout;
    VkPipelineLayout      build_cmds_pipeline_layout;
    VkPipeline            build_cmds_pipeline;

    VkDescriptorSet build_cmds_descriptor_set;

    VkDescriptorSetLayout draw_descriptor_set_layout;
    VkPipelineLayout      draw_pipeline_layout;
    VkPipeline            draw_pipeline;

    VkDescriptorSet draw_descriptor_set;

    GPUBuffer build_cmds_constants;

    std::vector<DebugMeshAlloc> proxy_mesh_allocs;
    u32                         index_buffer_offset;
    GPUBuffer                   index_buffer;
    u32                         vertex_buffer_offset;
    GPUBuffer                   vertex_buffer_position;
};

struct VulkanBackend;
struct ShaderModules;

DebugGeometryPassResources create_debug_geometry_pass_resources(VulkanBackend&       backend,
                                                                const ShaderModules& shader_modules);
void destroy_debug_geometry_pass_resources(VulkanBackend& backend, DebugGeometryPassResources& resources);

struct PreparedData;

void upload_debug_geometry_build_cmds_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                                           const DebugGeometryPassResources& resources);

class DescriptorWriteHelper;
struct FrameGraphBuffer;

void update_debug_geometry_build_cmds_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                                           const DebugGeometryPassResources& resources,
                                                           const FrameGraphBuffer&           draw_counter,
                                                           const FrameGraphBuffer&           user_commands,
                                                           const FrameGraphBuffer&           draw_commands,
                                                           const FrameGraphBuffer&           instance_buffer);

struct CommandBuffer;
struct FrameGraphTexture;

void record_debug_geometry_build_cmds_command_buffer(CommandBuffer&                    cmdBuffer,
                                                     const DebugGeometryPassResources& resources);

void update_debug_geometry_draw_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                                     const DebugGeometryPassResources& resources,
                                                     const FrameGraphBuffer&           instance_buffer);

void record_debug_geometry_draw_command_buffer(CommandBuffer& cmdBuffer, const DebugGeometryPassResources& resources,
                                               const FrameGraphTexture& hdr_buffer,
                                               const FrameGraphTexture& depth_texture,
                                               const FrameGraphBuffer&  draw_counter,
                                               const FrameGraphBuffer&  draw_commands);
} // namespace Reaper
