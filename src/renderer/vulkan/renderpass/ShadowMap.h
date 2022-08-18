////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"

#include <vulkan_loader/Vulkan.h>

#include "ShadowConstants.h"

#include <glm/vec2.hpp>
#include <nonstd/span.hpp>

#include <vector>

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
struct GPUTextureProperties;
struct ShaderModules;

struct ShadowMapPipelineInfo
{
    VkPipeline            pipeline;
    VkPipelineLayout      pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
};

struct ShadowMapResources
{
    ShadowMapPipelineInfo pipe;

    BufferInfo passConstantBuffer;
    BufferInfo instanceConstantBuffer;

    std::vector<VkDescriptorSet> descriptor_sets;
};

ShadowMapResources create_shadow_map_resources(ReaperRoot& root, VulkanBackend& backend,
                                               const ShaderModules& shader_modules);
void               destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources);

struct PreparedData;

void upload_shadow_map_resources(VulkanBackend& backend, const PreparedData& prepared, ShadowMapResources& resources);

struct DescriptorWriteHelper;

void update_shadow_map_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                            ShadowMapResources& resources, BufferInfo& vertex_position_buffer);

std::vector<GPUTextureProperties> fill_shadow_map_properties(const PreparedData& prepared);

struct CommandBuffer;
struct CullResources;
struct FrameGraphTexture;

void record_shadow_map_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                      ShadowMapResources&                         resources,
                                      const nonstd::span<const FrameGraphTexture> shadow_maps,
                                      const CullResources&                        cull_resources);
} // namespace Reaper
