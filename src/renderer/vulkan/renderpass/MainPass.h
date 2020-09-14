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

namespace Reaper
{
struct BlitPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;

VkRenderPass create_main_pass(ReaperRoot& /*root*/, VulkanBackend& backend,
                              const GPUTextureProperties& depthProperties);

BlitPipelineInfo create_main_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass);

struct MainPassResources
{
    BufferInfo drawPassConstantBuffer;
    BufferInfo drawInstanceConstantBuffer;

    ImageInfo   depthBuffer;
    VkImageView depthBufferView;
};

MainPassResources create_main_pass_resources(ReaperRoot& root, VulkanBackend& backend, glm::uvec2 extent);
void              destroy_main_pass_resources(VulkanBackend& backend, MainPassResources& resources);

void resize_main_pass_depth_buffer(ReaperRoot& root, VulkanBackend& backend, MainPassResources& resources,
                                   glm::uvec2 extent);
} // namespace Reaper
