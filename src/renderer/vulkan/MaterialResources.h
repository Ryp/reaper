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

#include <nonstd/span.hpp>
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

struct TextureResource
{
    ImageInfo   texture;
    VkImageView default_view;
};

struct MaterialResources
{
    ResourceStagingArea staging;

    std::vector<TextureResource> textures;

    VkSampler diffuseMapSampler;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSet       descriptor_set;
};

struct VulkanBackend;
struct ReaperRoot;

MaterialResources create_material_resources(ReaperRoot& root, VulkanBackend& backend);
void              destroy_material_resources(VulkanBackend& backend, MaterialResources& resources);

void load_textures(ReaperRoot& root, VulkanBackend& backend, MaterialResources& resources,
                   nonstd::span<const char*> texture_filenames, nonstd::span<TextureHandle> output_handles);

// handles will contain which texture to bind for each descriptor slot
void update_material_descriptor_set(VulkanBackend& backend, const MaterialResources& resources,
                                    const nonstd::span<TextureHandle> handles);

void record_material_upload_command_buffer(VulkanBackend& backend, ResourceStagingArea& staging,
                                           VkCommandBuffer cmdBuffer);
} // namespace Reaper
