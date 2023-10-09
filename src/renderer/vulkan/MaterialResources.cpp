////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MaterialResources.h"

#include "Backend.h"
#include "Barrier.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "GpuProfile.h"
#include "Image.h"
#include "TextureLoadingDDS.h"
#include "TextureLoadingPNG.h"

#include <core/Assert.h>
#include <core/Literals.h>

#include <span>

namespace Reaper
{
namespace
{
    void flush_pending_staging_commands(CommandBuffer& cmdBuffer, const ResourceStagingArea& staging,
                                        const StagingEntry& entry)
    {
        const GPUTextureAccess src = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED};
        const GPUTextureAccess dst = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};

        const GPUTextureSubresource default_subresource = default_texture_subresource(entry.texture_properties);
        const VkImageMemoryBarrier2 barrier = get_vk_image_barrier(entry.target, default_subresource, src, dst);

        const VkDependencyInfo dependencies = get_vk_image_barrier_depency_info(std::span(&barrier, 1));

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

    TextureResource create_texture_resource(VulkanBackend& backend, MaterialResources& resources,
                                            const char* debug_name, StagingEntry& staging_entry)
    {
        GPUTexture image_info =
            create_image(backend.device, debug_name, staging_entry.texture_properties, backend.vma_instance);

        staging_entry.target = image_info.handle;
        resources.staging.staging_queue.push_back(staging_entry);

        const GPUTextureView view = default_texture_view(staging_entry.texture_properties);

        return {
            .texture = image_info,
            .default_view = create_image_view(backend.device, image_info.handle, view),
        };
    }

    TextureResource load_texture_to_staging_dds(VulkanBackend& backend, MaterialResources& resources,
                                                const char* filename)
    {
        StagingEntry staging_entry = copy_texture_to_staging_area_dds(backend, resources.staging, filename);

        return create_texture_resource(backend, resources, filename, staging_entry);
    }

    TextureResource load_texture_to_staging_png(VulkanBackend& backend, MaterialResources& resources,
                                                const char* filename, bool is_srgb)
    {
        StagingEntry staging_entry = copy_texture_to_staging_area_png(backend, resources.staging, filename, is_srgb);

        return create_texture_resource(backend, resources, filename, staging_entry);
    }
} // namespace

MaterialResources create_material_resources(VulkanBackend& backend)
{
    constexpr u32 StagingBufferSizeBytes = static_cast<u32>(512_MiB);

    const GPUBufferProperties properties =
        DefaultGPUBufferProperties(StagingBufferSizeBytes, sizeof(u8), GPUBufferUsage::TransferSrc);

    GPUBuffer staging_buffer =
        create_buffer(backend.device, "Texture Staging Buffer", properties, backend.vma_instance, MemUsage::CPU_Only);

    return MaterialResources{
        .staging =
            ResourceStagingArea{
                .offset_bytes = 0,
                .buffer_properties = properties,
                .staging_buffer = staging_buffer,
                .bufferCopyRegions = {},
                .staging_queue = {},
            },
        .textures = {},
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

void load_dds_textures_to_staging(VulkanBackend& backend, MaterialResources& resources,
                                  std::span<std::string> texture_filenames, HandleSpan<TextureHandle> handle_span)
{
    Assert(handle_span.count == texture_filenames.size());

    for (u32 i = 0; i < texture_filenames.size(); i++)
    {
        const TextureHandle handle = TextureHandle(handle_span.offset + i);

        resources.textures[handle] = load_texture_to_staging_dds(backend, resources, texture_filenames[i].c_str());
    }
}

void load_png_textures_to_staging(VulkanBackend& backend, MaterialResources& resources,
                                  std::span<std::string> texture_filenames, HandleSpan<TextureHandle> handle_span,
                                  std::span<u32> is_srgb)
{
    Assert(handle_span.count == texture_filenames.size());

    for (u32 i = 0; i < texture_filenames.size(); i++)
    {
        const TextureHandle handle = TextureHandle(handle_span.offset + i);

        resources.textures[handle] =
            load_texture_to_staging_png(backend, resources, texture_filenames[i].c_str(), is_srgb[i] != 0);
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
            const GPUTextureAccess dst = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                                              | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                          VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};

            const GPUTextureSubresource default_subresource = default_texture_subresource(entry.texture_properties);
            const VkImageMemoryBarrier2 barrier = get_vk_image_barrier(entry.target, default_subresource, src, dst);

            prerender_barriers.emplace_back(barrier);
        }

        const VkDependencyInfo dependencies = get_vk_image_barrier_depency_info(prerender_barriers);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    flush_staging_area_state(staging);
}
} // namespace Reaper
