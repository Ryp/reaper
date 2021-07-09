////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
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

MainPipelineInfo create_main_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass);

struct MainPassResources
{
    BufferInfo drawPassConstantBuffer;
    BufferInfo drawInstanceConstantBuffer;

    ImageInfo   hdrBuffer;
    VkImageView hdrBufferView;
    ImageInfo   depthBuffer;
    VkImageView depthBufferView;

    VkFramebuffer main_framebuffer;

    VkRenderPass mainRenderPass;

    MainPipelineInfo mainPipe;

    VkSampler shadowMapSampler;
    VkSampler diffuseMapSampler;

    VkDescriptorSet descriptor_set;
    VkDescriptorSet material_descriptor_set;
};

MainPassResources create_main_pass_resources(ReaperRoot& root, VulkanBackend& backend, glm::uvec2 extent);
void              destroy_main_pass_resources(VulkanBackend& backend, MainPassResources& resources);

void resize_main_pass_resources(ReaperRoot& root, VulkanBackend& backend, MainPassResources& resources,
                                glm::uvec2 extent);

struct MaterialResources;

void update_main_pass_descriptor_sets(VulkanBackend& backend, const MainPassResources& resources,
                                      const MaterialResources&        material_resources,
                                      const nonstd::span<VkImageView> shadow_map_views);

struct PreparedData;

void upload_main_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                      MainPassResources& pass_resources);

struct CommandBuffer;
struct CullResources;
struct MeshCache;

void record_main_pass_command_buffer(CommandBuffer& cmdBuffer, VulkanBackend& backend, const PreparedData& prepared,
                                     const MainPassResources& pass_resources, const CullResources& cull_resources,
                                     const MeshCache& mesh_cache, VkExtent2D backbufferExtent);
} // namespace Reaper
