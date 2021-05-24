////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

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
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

MainPipelineInfo create_main_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass,
                                      VkDescriptorSetLayout material_descriptor_set_layout);

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

    VkDescriptorSet descriptor_set;
};

MainPassResources create_main_pass_resources(ReaperRoot& root, VulkanBackend& backend, glm::uvec2 extent,
                                             VkDescriptorSetLayout material_descriptor_set_layout);
void              destroy_main_pass_resources(VulkanBackend& backend, MainPassResources& resources);

void resize_main_pass_resources(ReaperRoot& root, VulkanBackend& backend, MainPassResources& resources,
                                glm::uvec2 extent);

void update_main_pass_descriptor_set(VulkanBackend& backend, const MainPassResources& resources,
                                     const nonstd::span<VkImageView> shadow_map_views);

struct PreparedData;

void upload_main_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                      MainPassResources& pass_resources);

struct CullOptions;
struct CullResources;
struct MeshCache;
struct MaterialResources;

void record_main_pass_command_buffer(const CullOptions& cull_options, VkCommandBuffer cmdBuffer,
                                     const PreparedData& prepared, const MainPassResources& pass_resources,
                                     const CullResources& cull_resources, const MaterialResources& material_resources,
                                     const MeshCache& mesh_cache, VkExtent2D backbufferExtent);
} // namespace Reaper
