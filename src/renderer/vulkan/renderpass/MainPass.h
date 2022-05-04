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
#include "renderer/vulkan/api/Vulkan.h"

#include <glm/vec2.hpp>

#include <nonstd/span.hpp>

#include <vector>

namespace Reaper
{
constexpr PixelFormat MainHDRColorFormat = PixelFormat::B10G11R11_UFLOAT_PACK32;
constexpr PixelFormat MainDepthFormat = PixelFormat::D16_UNORM;

struct MainPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSetLayout descSetLayout2;
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

struct MainPassResources
{
    BufferInfo drawPassConstantBuffer;
    BufferInfo drawInstanceConstantBuffer;

    MainPipelineInfo mainPipe;

    VkSampler shadowMapSampler;
    VkSampler diffuseMapSampler;

    VkDescriptorSet descriptor_set;
    VkDescriptorSet material_descriptor_set;
};

MainPassResources create_main_pass_resources(ReaperRoot& root, VulkanBackend& backend);
void              destroy_main_pass_resources(VulkanBackend& backend, MainPassResources& resources);

struct MaterialResources;
struct MeshCache;

void update_main_pass_descriptor_sets(VulkanBackend& backend, const MainPassResources& resources,
                                      const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                      const nonstd::span<VkImageView> shadow_map_views);

struct PreparedData;

void upload_main_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                      MainPassResources& pass_resources);

struct CommandBuffer;
struct CullResources;

void record_main_pass_command_buffer(CommandBuffer& cmdBuffer, VulkanBackend& backend, const PreparedData& prepared,
                                     const MainPassResources& pass_resources, const CullResources& cull_resources,
                                     VkExtent2D backbufferExtent, VkImageView hdrBufferView,
                                     VkImageView depthBufferView);
} // namespace Reaper
