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

#include <vector>

namespace Reaper
{
struct StagingEntry
{
    GPUTextureProperties texture_properties;
    u32                  copy_command_offset;
    u32                  copy_command_count;
    VkImage              target;
};

struct ResourceStagingArea
{
    u32        offset_bytes; // FIXME u32 enough?
    BufferInfo staging_buffer;
    // Setup buffer copy regions for each mip level
    std::vector<VkBufferImageCopy> bufferCopyRegions;

    std::vector<StagingEntry> staging_queue;
};

struct MaterialResources
{
    ResourceStagingArea staging;

    ImageInfo   default_diffuse;
    VkImageView default_diffuse_view;

    ImageInfo   bricks_diffuse;
    VkImageView bricks_diffuse_view;
    ImageInfo   bricks_roughness;
    VkImageView bricks_roughness_view;

    VkSampler diffuseMapSampler;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSet       descriptor_set;
};

struct VulkanBackend;
struct ReaperRoot;

MaterialResources create_material_resources(ReaperRoot& root, VulkanBackend& backend);
void              destroy_material_resources(VulkanBackend& backend, const MaterialResources& resources);

void record_material_upload_command_buffer(VulkanBackend& backend, ResourceStagingArea& staging,
                                           VkCommandBuffer cmdBuffer);

void update_material_descriptor_set(VulkanBackend& backend, const MaterialResources& resources);
} // namespace Reaper
