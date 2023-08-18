////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/ResourceHandle.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"

#include <vulkan_loader/Vulkan.h>

#include <glm/vec2.hpp>

#include <span>

#include <vector>

namespace Reaper
{
struct GBufferPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout desc_set_layout;
};

struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

struct GBufferPassResources
{
    GPUBuffer instancesConstantBuffer;

    GBufferPipelineInfo pipe;

    VkDescriptorSet descriptor_set;
};

GBufferPassResources create_gbuffer_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                   const ShaderModules& shader_modules);
void                 destroy_gbuffer_pass_resources(VulkanBackend& backend, GBufferPassResources& resources);

class DescriptorWriteHelper;
struct MaterialResources;
struct MeshCache;
struct SamplerResources;
struct FrameGraphBuffer;

void update_gbuffer_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const GBufferPassResources& resources,
                                         const FrameGraphBuffer&  visible_meshlet_buffer,
                                         const SamplerResources&  sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache);

struct PreparedData;

void upload_gbuffer_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                         GBufferPassResources& pass_resources);

struct CommandBuffer;
struct FrameGraphTexture;

void record_gbuffer_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                        const GBufferPassResources& pass_resources,
                                        const FrameGraphBuffer&     meshlet_counters,
                                        const FrameGraphBuffer&     meshlet_indirect_draw_commands,
                                        const FrameGraphBuffer&     meshlet_visible_index_buffer,
                                        const FrameGraphTexture& gbuffer_rt0, const FrameGraphTexture& gbuffer_rt1,
                                        const FrameGraphTexture& depth_buffer);
} // namespace Reaper
