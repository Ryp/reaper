////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TextureLoadingPNG.h"

#include "Backend.h"
#include "Image.h"
#include "MaterialResources.h"

#include <core/Assert.h>

#include <fmt/format.h>
#include <lodepng.h>

namespace Reaper
{
enum class PNGMode
{
    RGB,
    RGBA,
};

// Opens a texture file on the CPU, copies all regions to a single staging buffer and record the copy commands for
// the next steps.
StagingEntry copy_texture_to_staging_area_png(VulkanBackend& backend, ResourceStagingArea& staging,
                                              const char* file_path, bool is_srgb)
{
    u32         error;
    u8*         png_image_ptr = 0;
    u32         width;
    u32         height;
    u32         size_bytes;
    PixelFormat pixel_format;

    // NOTE: driver support for linear tiled RGB textures is more limited than the 4-channel version, so we prefer it.
    PNGMode mode = PNGMode::RGBA;

    switch (mode)
    {
    case PNGMode::RGB:
        error = lodepng_decode24_file(&png_image_ptr, &width, &height, file_path);
        size_bytes = width * height * 3;
        pixel_format = is_srgb ? PixelFormat::R8G8B8_SRGB : PixelFormat::R8G8B8_UNORM;
        break;
    case PNGMode::RGBA:
        error = lodepng_decode32_file(&png_image_ptr, &width, &height, file_path);
        size_bytes = width * height * 4;
        pixel_format = is_srgb ? PixelFormat::R8G8B8A8_SRGB : PixelFormat::R8G8B8A8_UNORM;
        break;
    }

    Assert(!error, fmt::format("error {}: {}", error, lodepng_error_text(error)));

    GPUTextureProperties properties = default_texture_properties(
        width, height, pixel_format, GPUTextureUsage::TransferDst | GPUTextureUsage::Sampled);
    properties.misc_flags = GPUTextureMisc::LinearTiling;

    upload_buffer_data(backend.device, backend.vma_instance, staging.staging_buffer, staging.buffer_properties,
                       png_image_ptr, size_bytes, staging.offset_bytes);

    const GPUTextureSubresource subresource = default_texture_subresource_one_color_mip();

    const u32 command_offset = staging.bufferCopyRegions.size();

    // Setup a buffer image copy structure for the current mip level
    staging.bufferCopyRegions.emplace_back(
        VkBufferImageCopy2{.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                           .pNext = nullptr,
                           .bufferOffset = staging.offset_bytes,
                           .bufferRowLength = 0,
                           .bufferImageHeight = 0,
                           .imageSubresource = get_vk_image_subresource_layers(subresource),
                           .imageOffset = {.x = 0, .y = 0, .z = 0},
                           .imageExtent = {.width = width, .height = height, .depth = 1}});

    // Keep track of offset
    staging.offset_bytes += size_bytes;

    Assert(staging.offset_bytes < staging.buffer_properties.element_count, "OOB");

    free(png_image_ptr);

    return StagingEntry{
        .texture_properties = properties,
        .copy_command_offset = command_offset,
        .copy_command_count = 1,
        .target = VK_NULL_HANDLE, // FIXME
    };
}
} // namespace Reaper
