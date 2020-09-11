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

VkRenderPass create_shadow_raster_pass(ReaperRoot& root, VulkanBackend& backend,
                                       const GPUTextureProperties& shadowMapProperties);

ShadowMapPipelineInfo create_shadow_map_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass,
                                                 u32 shadowMapRes);

struct ShadowMapResources
{
    VkRenderPass shadowMapPass;

    BufferInfo shadowMapPassConstantBuffer;
    BufferInfo shadowMapInstanceConstantBuffer;

    ImageInfo     shadowMap;
    VkImageView   shadowMapView;
    VkFramebuffer shadowMapFramebuffer;
    VkSampler     shadowMapSampler;
};

ShadowMapResources create_shadow_map_resources(ReaperRoot& root, VulkanBackend& backend);
void               destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources);
} // namespace Reaper
