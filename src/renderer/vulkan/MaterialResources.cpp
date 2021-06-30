////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MaterialResources.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Image.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/Profile.h"

#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>

#include <cfloat> // FIXME
#include <nonstd/span.hpp>

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
        case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm:
            return PixelFormat::B8G8R8A8_UNORM;
        default:
            AssertUnreachable();
            return PixelFormat::Unknown;
        }
    }

    StagingEntry copy_texture_to_staging_area(ReaperRoot& root, VulkanBackend& backend, ResourceStagingArea& staging,
                                              const char* file_path)
    {
        tinyddsloader::DDSFile dds;

        auto ret = dds.Load(file_path);

        Assert(ret == tinyddsloader::Result::Success);
        Assert(dds.GetTextureDimension() == tinyddsloader::DDSFile::TextureDimension::Texture2D);
        Assert(dds.GetDepth() == 1);
        Assert(dds.GetArraySize() == 1);

        const u32 command_offset = staging.bufferCopyRegions.size();

        for (uint32_t arrayIdx = 0; arrayIdx < dds.GetArraySize(); arrayIdx++)
        {
            for (uint32_t mipIdx = 0; mipIdx < dds.GetMipCount(); mipIdx++)
            {
                const auto* imageData = dds.GetImageData(mipIdx, arrayIdx);

                const void* input_data_ptr = imageData->m_mem;
                const u32   size_bytes = imageData->m_memSlicePitch;

                log_debug(root, "vulkan: cpu texture upload: mip = {}, array = {}, size = {}, offset = {}", mipIdx,
                          arrayIdx, size_bytes, staging.offset_bytes);

                upload_buffer_data(backend.device, backend.vma_instance, staging.staging_buffer, input_data_ptr,
                                   size_bytes, staging.offset_bytes);

                // Setup a buffer image copy structure for the current mip level
                VkBufferImageCopy& bufferCopyRegion = staging.bufferCopyRegions.emplace_back();
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = mipIdx;
                bufferCopyRegion.imageSubresource.baseArrayLayer = arrayIdx;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = imageData->m_width;
                bufferCopyRegion.imageExtent.height = imageData->m_height;
                bufferCopyRegion.imageExtent.depth = imageData->m_depth;
                bufferCopyRegion.bufferOffset = staging.offset_bytes;

                // Keep track of offset
                staging.offset_bytes += size_bytes;

                Assert(staging.offset_bytes < staging.staging_buffer.descriptor.elementCount); // OOB!
            }
        }

        const PixelFormat pixel_format = get_dds_pixel_format(dds.GetFormat());

        GPUTextureProperties properties = DefaultGPUTextureProperties(dds.GetWidth(), dds.GetHeight(), pixel_format);
        properties.depth = dds.GetDepth();
        properties.mipCount = dds.GetMipCount();
        properties.layerCount = dds.GetArraySize();
        properties.usageFlags = GPUTextureUsage::Sampled | GPUTextureUsage::TransferDst;

        const u32 command_count = staging.bufferCopyRegions.size() - command_offset;

        return {
            properties, command_offset, command_count,
            VK_NULL_HANDLE, // FIXME
        };
    }

    void flush_pending_staging_commands(CommandBuffer& cmdBuffer, const ResourceStagingArea& staging,
                                        const StagingEntry& entry)
    {
        // The sub resource range describes the regions of the image that will be transitioned using the memory
        // barriers below Transition the texture image layout to transfer target, so we can safely copy our buffer
        // data to it.
        VkImageMemoryBarrier imageMemoryBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0,
            0,
            entry.target,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

        // Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
        // Source pipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
        // Destination pipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
        vkCmdPipelineBarrier(cmdBuffer.handle, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        const nonstd::span<const VkBufferImageCopy> copy_regions(&staging.bufferCopyRegions[entry.copy_command_offset],
                                                                 entry.copy_command_count);

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(cmdBuffer.handle,
                               staging.staging_buffer.buffer,
                               entry.target,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<u32>(copy_regions.size()),
                               copy_regions.data());
    }

    VkImageMemoryBarrier prerender_barrier(VkImage output_image)
    {
        return VkImageMemoryBarrier{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            0,
            output_image,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};
    }

    void flush_staging_area_state(ResourceStagingArea& staging)
    {
        staging.offset_bytes = 0;
        staging.bufferCopyRegions.clear();
        staging.staging_queue.clear();
    }

    TextureHandle load_texture(ReaperRoot& root, VulkanBackend& backend, MaterialResources& resources,
                               const char* filename)
    {
        const TextureHandle resource_index = static_cast<TextureHandle>(resources.textures.size());
        TextureResource&    new_texture = resources.textures.emplace_back();

        StagingEntry staging_entry = copy_texture_to_staging_area(root, backend, resources.staging, filename);

        ImageInfo image_info =
            create_image(root, backend.device, filename, staging_entry.texture_properties, backend.vma_instance);

        staging_entry.target = image_info.handle;
        resources.staging.staging_queue.push_back(staging_entry);

        new_texture.texture = image_info;
        new_texture.default_view = create_default_image_view(root, backend.device, image_info);

        return resource_index;
    }
} // namespace

MaterialResources create_material_resources(ReaperRoot& root, VulkanBackend& backend)
{
    constexpr u32 StagingBufferSizeBytes = 8_MiB;

    BufferInfo staging_buffer =
        create_buffer(root, backend.device, "Staging Buffer",
                      DefaultGPUBufferProperties(StagingBufferSizeBytes, sizeof(u8), GPUBufferUsage::TransferSrc),
                      backend.vma_instance, MemUsage::CPU_Only);

    ResourceStagingArea staging = {};
    staging.offset_bytes = 0;
    staging.staging_buffer = staging_buffer;

    log_debug(root, "vulkan: copy texture to staging texture");

    return MaterialResources{
        staging,
        {},
        {},
    };
}

void destroy_material_resources(VulkanBackend& backend, MaterialResources& resources)
{
    for (const auto& texture : resources.textures)
    {
        vkDestroyImageView(backend.device, texture.default_view, nullptr);
        vmaDestroyImage(backend.vma_instance, texture.texture.handle, texture.texture.allocation);
    }
    resources.textures.clear();

    vmaDestroyBuffer(backend.vma_instance, resources.staging.staging_buffer.buffer,
                     resources.staging.staging_buffer.allocation);
}

void load_textures(ReaperRoot& root, VulkanBackend& backend, MaterialResources& resources,
                   nonstd::span<const char*> texture_filenames, nonstd::span<TextureHandle> output_handles)
{
    Assert(output_handles.size() >= texture_filenames.size());

    for (u32 i = 0; i < texture_filenames.size(); i++)
    {
        output_handles[i] = load_texture(root, backend, resources, texture_filenames[i]);
    }
}

void record_material_upload_command_buffer(ResourceStagingArea& staging, CommandBuffer& cmdBuffer)
{
    if (staging.staging_queue.empty())
        return;

    {
        REAPER_PROFILE_SCOPE_GPU("Upload", MP_DARKGOLDENROD);

        for (const auto& entry : staging.staging_queue)
        {
            flush_pending_staging_commands(cmdBuffer, staging, entry);
        }
    }

    {
        REAPER_PROFILE_SCOPE_GPU("Image Barriers", MP_RED);

        std::vector<VkImageMemoryBarrier> prerender_barriers;

        for (const auto& entry : staging.staging_queue)
        {
            prerender_barriers.push_back(prerender_barrier(entry.target));
        }

        // Insert a memory dependency at the proper pipeline stages that will execute the image layout
        // transition Source pipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
        // Destination pipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
        vkCmdPipelineBarrier(cmdBuffer.handle, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr, 0, nullptr, static_cast<u32>(prerender_barriers.size()),
                             prerender_barriers.data());
    }

    flush_staging_area_state(staging);
}
} // namespace Reaper
