////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TextureLoadingDDS.h"

#include "Backend.h"
#include "Image.h"
#include "MaterialResources.h"

#include <core/Assert.h>

#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>

namespace Reaper
{
namespace
{
    PixelFormat get_dds_pixel_format(tinyddsloader::DDSFile::DXGIFormat dds_format)
    {
        switch (dds_format)
        {
        case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm:
            return PixelFormat::BC1_RGB_UNORM_BLOCK; // FIXME Assume RGB without alpha
        case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB:
            return PixelFormat::BC1_RGB_SRGB_BLOCK; // FIXME Assume RGB without alpha
        case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm:
            return PixelFormat::BC2_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB:
            return PixelFormat::BC2_SRGB_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm:
            return PixelFormat::BC3_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB:
            return PixelFormat::BC3_SRGB_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm:
            return PixelFormat::BC4_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm:
            return PixelFormat::BC4_SNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm:
            return PixelFormat::BC5_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm:
            return PixelFormat::BC5_SNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC6H_SF16:
            return PixelFormat::BC6H_SFLOAT_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC6H_UF16:
            return PixelFormat::BC6H_UFLOAT_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm:
            return PixelFormat::BC7_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB:
            return PixelFormat::BC7_SRGB_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm:
            return PixelFormat::B8G8R8A8_UNORM;
        default:
            AssertUnreachable();
            return PixelFormat::Unknown;
        }
    }

} // namespace

// Opens a texture file on the CPU, copies all regions to a single staging buffer and record the copy commands for
// the next steps.
StagingEntry copy_texture_to_staging_area_dds(VulkanBackend& backend, ResourceStagingArea& staging,
                                              const char* file_path)
{
    tinyddsloader::DDSFile dds;

    auto ret = dds.Load(file_path);

    Assert(ret == tinyddsloader::Result::Success);
    Assert(dds.GetTextureDimension() == tinyddsloader::DDSFile::TextureDimension::Texture2D);
    Assert(dds.GetDepth() == 1);

    const u32 mip_count = dds.GetMipCount();
    const u32 layer_count = dds.GetArraySize();

    Assert(layer_count == 1);

    const PixelFormat pixel_format = get_dds_pixel_format(dds.GetFormat());

    GPUTextureProperties properties = default_texture_properties(
        dds.GetWidth(), dds.GetHeight(), pixel_format, GPUTextureUsage::TransferDst | GPUTextureUsage::Sampled);
    properties.depth = dds.GetDepth();
    properties.mip_count = mip_count;
    properties.layer_count = layer_count;

    const u32 copy_count = mip_count * layer_count;
    const u32 command_offset = static_cast<u32>(staging.bufferCopyRegions.size());

    for (uint32_t arrayIdx = 0; arrayIdx < layer_count; arrayIdx++)
    {
        for (uint32_t mipIdx = 0; mipIdx < mip_count; mipIdx++)
        {
            const auto* imageData = dds.GetImageData(mipIdx, arrayIdx);

            const void* input_data_ptr = imageData->m_mem;
            const u32   size_bytes = imageData->m_memSlicePitch;

            upload_buffer_data(backend.device, backend.vma_instance, staging.staging_buffer, staging.buffer_properties,
                               input_data_ptr, size_bytes, staging.offset_bytes);

            const GPUTextureSubresource subresource = default_texture_subresource_one_color_mip(mipIdx, arrayIdx);

            // Setup a buffer image copy structure for the current mip level
            staging.bufferCopyRegions.emplace_back(VkBufferImageCopy2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .pNext = nullptr,
                .bufferOffset = staging.offset_bytes,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = get_vk_image_subresource_layers(subresource),
                .imageOffset = {.x = 0, .y = 0, .z = 0},
                .imageExtent = {
                    .width = imageData->m_width, .height = imageData->m_height, .depth = imageData->m_depth}});

            // Keep track of offset
            staging.offset_bytes += size_bytes;

            Assert(staging.offset_bytes < staging.buffer_properties.element_count, "OOB");
        }
    }

    return StagingEntry{
        .texture_properties = properties,
        .copy_command_offset = command_offset,
        .copy_command_count = copy_count,
        .target = VK_NULL_HANDLE, // FIXME
    };
}
} // namespace Reaper
