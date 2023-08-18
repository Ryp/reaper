////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MaterialResources.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"
#include <core/Assert.h>
#include <core/Literals.h>

#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>

#include <span>

#include <cfloat> // FIXME

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

                upload_buffer_data(backend.device, backend.vma_instance, staging.staging_buffer,
                                   staging.buffer_properties, input_data_ptr, size_bytes, staging.offset_bytes);

                // Setup a buffer image copy structure for the current mip level
                staging.bufferCopyRegions.emplace_back(VkBufferImageCopy2{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                    .pNext = nullptr,
                    .bufferOffset = staging.offset_bytes,
                    .bufferRowLength = 0,   // FIXME
                    .bufferImageHeight = 0, // FIXME
                    .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .mipLevel = mipIdx,
                                         .baseArrayLayer = arrayIdx,
                                         .layerCount = 1},
                    .imageOffset = {0, 0, 0},
                    .imageExtent = {
                        .width = imageData->m_width, .height = imageData->m_height, .depth = imageData->m_depth}});

                // Keep track of offset
                staging.offset_bytes += size_bytes;

                Assert(staging.offset_bytes < staging.buffer_properties.element_count, "OOB");
            }
        }

        const PixelFormat pixel_format = get_dds_pixel_format(dds.GetFormat());

        GPUTextureProperties properties = default_texture_properties(
            dds.GetWidth(), dds.GetHeight(), pixel_format, GPUTextureUsage::Sampled | GPUTextureUsage::TransferDst);
        properties.depth = dds.GetDepth();
        properties.mip_count = dds.GetMipCount();
        properties.layer_count = dds.GetArraySize();

        const u32 command_count = staging.bufferCopyRegions.size() - command_offset;

        return {
            properties, command_offset, command_count,
            VK_NULL_HANDLE, // FIXME
        };
    }

    void flush_pending_staging_commands(CommandBuffer& cmdBuffer, const ResourceStagingArea& staging,
                                        const StagingEntry& entry)
    {
        const GPUTextureAccess src = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED};
        const GPUTextureAccess dst = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};

        const GPUTextureSubresource default_subresource = default_texture_subresource(entry.texture_properties);
        const VkImageMemoryBarrier2 barrier = get_vk_image_barrier(entry.target, default_subresource, src, dst);

        const VkDependencyInfo dependencies = get_vk_image_barrier_depency_info(1, &barrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);

        std::span<const VkBufferImageCopy2> copy_regions(&staging.bufferCopyRegions[entry.copy_command_offset],
                                                         entry.copy_command_count);

        // Copy mip levels from staging buffer
        const VkCopyBufferToImageInfo2 copy = {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
            .pNext = nullptr,
            .srcBuffer = staging.staging_buffer.handle,
            .dstImage = entry.target,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = static_cast<u32>(copy_regions.size()),
            .pRegions = copy_regions.data(),
        };

        vkCmdCopyBufferToImage2(cmdBuffer.handle, &copy);
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

        GPUTexture image_info =
            create_image(root, backend.device, filename, staging_entry.texture_properties, backend.vma_instance);

        staging_entry.target = image_info.handle;
        resources.staging.staging_queue.push_back(staging_entry);

        new_texture.texture = image_info;

        const GPUTextureView default_view = default_texture_view(staging_entry.texture_properties);
        new_texture.default_view = create_image_view(root, backend.device, image_info, default_view);

        return resource_index;
    }
} // namespace

MaterialResources create_material_resources(ReaperRoot& root, VulkanBackend& backend)
{
    constexpr u32 StagingBufferSizeBytes = 8_MiB;

    const GPUBufferProperties properties =
        DefaultGPUBufferProperties(StagingBufferSizeBytes, sizeof(u8), GPUBufferUsage::TransferSrc);

    GPUBuffer staging_buffer =
        create_buffer(root, backend.device, "Staging Buffer", properties, backend.vma_instance, MemUsage::CPU_Only);

    ResourceStagingArea staging = {};
    staging.offset_bytes = 0;
    staging.staging_buffer = staging_buffer;
    staging.buffer_properties = properties;

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

    vmaDestroyBuffer(backend.vma_instance, resources.staging.staging_buffer.handle,
                     resources.staging.staging_buffer.allocation);
}

void load_textures(ReaperRoot& root, VulkanBackend& backend, MaterialResources& resources,
                   std::span<const char*> texture_filenames, std::span<TextureHandle> output_handles)
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

    for (const auto& entry : staging.staging_queue)
    {
        flush_pending_staging_commands(cmdBuffer, staging, entry);
    }

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Image Barriers", Color::Red);

        std::vector<VkImageMemoryBarrier2> prerender_barriers;

        for (const auto& entry : staging.staging_queue)
        {
            const GPUTextureAccess src = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};
            const GPUTextureAccess dst = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                          VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};

            const GPUTextureSubresource default_subresource = default_texture_subresource(entry.texture_properties);
            const VkImageMemoryBarrier2 barrier = get_vk_image_barrier(entry.target, default_subresource, src, dst);

            prerender_barriers.emplace_back(barrier);
        }

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(prerender_barriers.size()), prerender_barriers.data());

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    flush_staging_area_state(staging);
}
} // namespace Reaper
