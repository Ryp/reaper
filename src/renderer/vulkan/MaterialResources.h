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

#include <vulkan_loader/Vulkan.h>

#include <span>
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
    u32                 offset_bytes; // FIXME u32 enough?
    GPUBufferProperties buffer_properties;
    GPUBuffer           staging_buffer;
    // Setup buffer copy regions for each mip level
    std::vector<VkBufferImageCopy2> bufferCopyRegions;

    std::vector<StagingEntry> staging_queue;
};

struct TextureResource
{
    GPUTexture  texture;
    VkImageView default_view;
};

struct MaterialResources
{
    ResourceStagingArea staging;

    std::vector<TextureResource> textures;
    std::vector<TextureHandle>   texture_handles;
};

struct VulkanBackend;

MaterialResources create_material_resources(VulkanBackend& backend);
void              destroy_material_resources(VulkanBackend& backend, MaterialResources& resources);

enum class TextureFileFormat
{
    DDS,
    PNG,
};

REAPER_RENDERER_API void load_textures(VulkanBackend& backend, MaterialResources& resources,
                                       TextureFileFormat file_format, std::span<const char*> texture_filenames,
                                       std::span<TextureHandle> output_handles);

struct CommandBuffer;

void record_material_upload_command_buffer(ResourceStagingArea& staging, CommandBuffer& cmdBuffer);
} // namespace Reaper
