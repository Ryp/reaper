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
};

struct VulkanBackend;

MaterialResources create_material_resources(VulkanBackend& backend);
void              destroy_material_resources(VulkanBackend& backend, MaterialResources& resources);

inline HandleSpan<TextureHandle> alloc_material_textures(MaterialResources& resources, u32 count)
{
    const u32 old_size = resources.textures.size();

    resources.textures.resize(old_size + count);

    return HandleSpan<TextureHandle>{
        .offset = old_size,
        .count = count,
    };
}

REAPER_RENDERER_API void load_dds_textures_to_staging(VulkanBackend& backend, MaterialResources& resources,
                                                      std::span<std::string>    texture_filenames,
                                                      HandleSpan<TextureHandle> handle_span);

REAPER_RENDERER_API void load_png_textures_to_staging(VulkanBackend& backend, MaterialResources& resources,
                                                      std::span<std::string>    texture_filenames,
                                                      HandleSpan<TextureHandle> handle_span, std::span<u32> is_srgb);

struct CommandBuffer;

void record_material_upload_command_buffer(ResourceStagingArea& staging, CommandBuffer& cmdBuffer);
} // namespace Reaper
