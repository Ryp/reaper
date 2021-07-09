////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/api/Vulkan.h"

#include "ShadowConstants.h"

#include <glm/vec2.hpp>

#include <array>
#include <vector>

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

struct ShadowMapPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct ShadowMapResources
{
    VkRenderPass shadowMapPass;

    ShadowMapPipelineInfo pipe;

    BufferInfo shadowMapPassConstantBuffer;
    BufferInfo shadowMapInstanceConstantBuffer;

    std::vector<VkDescriptorSet> descriptor_sets;

    // These are valid for ONE FRAME ONLY
    std::vector<ImageInfo>     shadowMap;
    std::vector<VkImageView>   shadowMapView;
    std::vector<VkFramebuffer> shadowMapFramebuffer;
};

GPUTextureProperties get_shadow_map_texture_properties(glm::uvec2 size);

VkFramebuffer create_shadow_map_framebuffer(VulkanBackend& backend, VkRenderPass renderPass,
                                            const GPUTextureProperties& properties);

ShadowMapResources create_shadow_map_resources(ReaperRoot& root, VulkanBackend& backend);
void               destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources);

struct PreparedData;

void prepare_shadow_map_objects(ReaperRoot& root, VulkanBackend& backend, const PreparedData& prepared,
                                ShadowMapResources& pass_resources);
void upload_shadow_map_resources(VulkanBackend& backend, const PreparedData& prepared, ShadowMapResources& resources);

void update_shadow_map_pass_descriptor_sets(VulkanBackend& backend, const PreparedData& prepared,
                                            ShadowMapResources& pass_resources);

struct CommandBuffer;
struct CullResources;

void record_shadow_map_command_buffer(CommandBuffer& cmdBuffer, VulkanBackend& backend, const PreparedData& prepared,
                                      ShadowMapResources& resources, const CullResources& cull_resources,
                                      VkBuffer vertex_position_buffer);
} // namespace Reaper
