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

#include <nonstd/span.hpp>

#include <vector>

namespace Reaper
{
struct VisibilityBufferPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout desc_set_layout;
};

struct ReaperRoot;
struct VulkanBackend;
struct ShaderModules;

struct VisibilityBufferPassResources
{
    BufferInfo instancesConstantBuffer;

    VisibilityBufferPipelineInfo pipe;
    VkDescriptorSet              descriptor_set;

    VisibilityBufferPipelineInfo fill_pipe;
    VkDescriptorSet              descriptor_set_fill;
};

VisibilityBufferPassResources create_vis_buffer_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                               const ShaderModules& shader_modules);
void destroy_vis_buffer_pass_resources(VulkanBackend& backend, VisibilityBufferPassResources& resources);

struct MaterialResources;
struct MeshletCullingResources;
struct MeshCache;
struct SamplerResources;
class DescriptorWriteHelper;
struct FrameGraphTexture;

struct PreparedData;

void update_vis_buffer_pass_descriptor_sets(DescriptorWriteHelper&               write_helper,
                                            const VisibilityBufferPassResources& resources,
                                            const PreparedData&                  prepared,
                                            const SamplerResources&              sampler_resources,
                                            const MaterialResources&             material_resources,
                                            const MeshletCullingResources&       meshlet_culling_resources,
                                            const MeshCache&                     mesh_cache,
                                            const FrameGraphTexture&             vis_buffer,
                                            const FrameGraphTexture&             gbuffer_rt0,
                                            const FrameGraphTexture&             gbuffer_rt1);

void upload_vis_buffer_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                            VisibilityBufferPassResources& pass_resources);

struct CommandBuffer;
struct MeshletCullingResources;
struct FrameGraphTexture;

void record_vis_buffer_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                           const VisibilityBufferPassResources& pass_resources,
                                           const MeshletCullingResources&       meshlet_culling_resources,
                                           const FrameGraphTexture& vis_buffer, const FrameGraphTexture& depth_buffer);

void record_fill_gbuffer_pass_command_buffer(CommandBuffer& cmdBuffer, const VisibilityBufferPassResources& resources,
                                             VkExtent2D backbufferExtent);
} // namespace Reaper
