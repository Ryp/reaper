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
constexpr PixelFormat ForwardHDRColorFormat = PixelFormat::B10G11R11_UFLOAT_PACK32;
constexpr PixelFormat ForwardDepthFormat = PixelFormat::D16_UNORM;

struct ForwardPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSetLayout descSetLayout2;
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

struct ForwardPassResources
{
    BufferInfo passConstantBuffer;
    BufferInfo instancesConstantBuffer;

    ForwardPipelineInfo pipe;

    VkDescriptorSet descriptor_set;
    VkDescriptorSet material_descriptor_set;
};

ForwardPassResources create_forward_pass_resources(ReaperRoot& root, VulkanBackend& backend);
void                 destroy_forward_pass_resources(VulkanBackend& backend, ForwardPassResources& resources);

struct MaterialResources;
struct MeshCache;
struct LightingPassResources;
struct SamplerResources;

void update_forward_pass_descriptor_sets(VulkanBackend& backend, const ForwardPassResources& resources,
                                         const SamplerResources&  sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                         const LightingPassResources&    lighting_resources,
                                         const nonstd::span<VkImageView> shadow_map_views);

struct PreparedData;

void upload_forward_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                         ForwardPassResources& pass_resources);

struct CommandBuffer;
struct CullResources;

void record_forward_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                        const ForwardPassResources& pass_resources, const CullResources& cull_resources,
                                        VkExtent2D backbufferExtent, VkImageView hdrBufferView,
                                        VkImageView depthBufferView);
} // namespace Reaper
