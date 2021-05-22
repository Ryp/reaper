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

struct ShadowPassResources
{
    VkDescriptorSet descriptor_set;
};

struct ShadowMapResources
{
    VkRenderPass shadowMapPass;

    ShadowMapPipelineInfo pipe;

    BufferInfo shadowMapPassConstantBuffer;
    BufferInfo shadowMapInstanceConstantBuffer;

    // These are valid for ONE FRAME ONLY
    std::vector<ShadowPassResources> passes;

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

struct ShadowPassData;

ShadowPassResources create_shadow_map_pass_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                           const ShadowMapResources& resources,
                                                           const ShadowPassData&     shadow_pass);

struct PreparedData;

void shadow_map_prepare_buffers(VulkanBackend& backend, const PreparedData& prepared, ShadowMapResources& resources);

struct CullOptions;
struct CullResources;

void record_shadow_map_command_buffer(const CullOptions& cull_options, VkCommandBuffer cmdBuffer,
                                      VulkanBackend& backend, const PreparedData& prepared,
                                      ShadowMapResources& resources, const CullResources& cull_resources,
                                      VkBuffer vertex_buffer);
} // namespace Reaper
