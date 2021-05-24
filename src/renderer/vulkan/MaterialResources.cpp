////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MaterialResources.h"

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/SwapchainRendererBase.h"

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

    struct StagingEntry
    {
        GPUTextureProperties texture_properties;
        u32                  copy_command_offset;
        u32                  copy_command_count;
    };

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
            properties,
            command_offset,
            command_count,
        };
    }

    void flush_pending_staging_commands(VulkanBackend& backend, ResourceStagingArea& staging, VkCommandBuffer cmdBuffer,
                                        const StagingEntry& entry, VkImage output_image)
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
            backend.physicalDeviceInfo.graphicsQueueIndex,
            backend.physicalDeviceInfo.graphicsQueueIndex,
            output_image,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

        // Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
        // Source pipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
        // Destination pipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &imageMemoryBarrier);

        const nonstd::span<VkBufferImageCopy> copy_regions(&staging.bufferCopyRegions[entry.copy_command_offset],
                                                           entry.copy_command_count);

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(cmdBuffer,
                               staging.staging_buffer.buffer,
                               output_image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<u32>(copy_regions.size()),
                               copy_regions.data());
    }

    VkImageMemoryBarrier prerender_barrier(VulkanBackend& backend, VkImage output_image)
    {
        return VkImageMemoryBarrier{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            backend.physicalDeviceInfo.graphicsQueueIndex,
            backend.physicalDeviceInfo.graphicsQueueIndex,
            output_image,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};
    }

    VkDescriptorSet create_material_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                   VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1, &layout};

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

        return descriptor_set;
    }
} // namespace

MaterialResources create_material_resources(ReaperRoot& root, VulkanBackend& backend, VkCommandBuffer cmdBuffer)
{
    constexpr u32 StagingBufferSizeBytes = 4_MiB;

    BufferInfo staging_buffer =
        create_buffer(root, backend.device, "Staging Buffer",
                      DefaultGPUBufferProperties(StagingBufferSizeBytes, sizeof(u8), GPUBufferUsage::TransferSrc),
                      backend.vma_instance, MemUsage::CPU_Only);

    ResourceStagingArea staging = {};
    staging.offset_bytes = 0;
    staging.staging_buffer = staging_buffer;

    log_debug(root, "vulkan: copy texture to staging texture");

    const StagingEntry default_diffuse_entry =
        copy_texture_to_staging_area(root, backend, staging, "res/texture/default.dds");

    const StagingEntry bricks_diffuse_entry =
        copy_texture_to_staging_area(root, backend, staging, "res/texture/bricks_diffuse.dds");

    const StagingEntry bricks_roughness_entry =
        copy_texture_to_staging_area(root, backend, staging, "res/texture/bricks_specular.dds");

    ImageInfo default_diffuse = create_image(root, backend.device, "Default Diffuse Map",
                                             default_diffuse_entry.texture_properties, backend.vma_instance);
    ImageInfo bricks_diffuse = create_image(root, backend.device, "Bricks Diffuse Map",
                                            bricks_diffuse_entry.texture_properties, backend.vma_instance);
    ImageInfo bricks_roughness = create_image(root, backend.device, "Bricks Roughness Map",
                                              bricks_roughness_entry.texture_properties, backend.vma_instance);

    {
        VkCommandBufferBeginInfo cmdBufferBeginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            0,      // Not caring yet
            nullptr // No inheritance yet
        };

        Assert(vkResetCommandBuffer(cmdBuffer, 0) == VK_SUCCESS);

        Assert(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

#if defined(REAPER_USE_MICROPROFILE)
        MicroProfileThreadLogGpu* pGpuLog = MicroProfileThreadLogGpuAlloc();
        MICROPROFILE_GPU_BEGIN(cmdBuffer, pGpuLog);
        MICROPROFILE_GPU_ENTERI_L(pGpuLog, "GPU", "Upload", MP_BLUE2);
#endif

        {
            REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Upload", MP_DARKGOLDENROD);

            flush_pending_staging_commands(backend, staging, cmdBuffer, bricks_diffuse_entry, bricks_diffuse.handle);
            flush_pending_staging_commands(backend, staging, cmdBuffer, bricks_roughness_entry,
                                           bricks_roughness.handle);
            flush_pending_staging_commands(backend, staging, cmdBuffer, default_diffuse_entry, default_diffuse.handle);
        }

        {
            REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Image Barriers", MP_RED);

            std::vector<VkImageMemoryBarrier> prerender_barriers;

            prerender_barriers.push_back(prerender_barrier(backend, default_diffuse.handle));
            prerender_barriers.push_back(prerender_barrier(backend, bricks_diffuse.handle));
            prerender_barriers.push_back(prerender_barrier(backend, bricks_roughness.handle));

            // Insert a memory dependency at the proper pipeline stages that will execute the image layout
            // transition Source pipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
            // Destination pipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, static_cast<u32>(prerender_barriers.size()),
                                 prerender_barriers.data());
        }

#if defined(REAPER_USE_MICROPROFILE)
        MICROPROFILE_GPU_LEAVE_L(pGpuLog);

        const u64 microprofile_data = MicroProfileGpuEnd(pGpuLog);
        MicroProfileThreadLogGpuFree(pGpuLog);

        MICROPROFILE_GPU_SUBMIT(MicroProfileGetGlobalGpuQueue(), microprofile_data);
        MicroProfileFlip(cmdBuffer);
#endif

        Assert(vkEndCommandBuffer(cmdBuffer) == VK_SUCCESS);

        VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr};

        log_debug(root, "vulkan: submit commands");
        Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &submitInfo, nullptr) == VK_SUCCESS);

        log_debug(root, "vulkan: wait for idle queue");
        Assert(vkQueueWaitIdle(backend.deviceInfo.graphicsQueue) == VK_SUCCESS);
    }

    VkImageView default_diffuse_view = create_default_image_view(root, backend.device, default_diffuse);
    VkImageView bricks_diffuse_view = create_default_image_view(root, backend.device, bricks_diffuse);
    VkImageView bricks_roughness_view = create_default_image_view(root, backend.device, bricks_roughness);

    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.anisotropyEnable = VK_FALSE; // FIXME enable at higher level
    samplerCreateInfo.maxAnisotropy = 8;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.f;
    samplerCreateInfo.minLod = 0.f;
    samplerCreateInfo.maxLod = FLT_MAX;

    VkSampler sampler = VK_NULL_HANDLE;
    Assert(vkCreateSampler(backend.device, &samplerCreateInfo, nullptr, &sampler) == VK_SUCCESS);
    log_debug(root, "vulkan: created sampler with handle: {}", static_cast<void*>(sampler));

    constexpr u32 DiffuseMapMaxCount = 8; // FIXME

    std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBinding = {
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount,
                                     VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    std::array<VkDescriptorBindingFlags, 2> bindingFlags = {VK_FLAGS_NONE, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

    const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr, bindingFlags.size(),
        bindingFlags.data()};

    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &descriptorSetLayoutBindingFlags, 0,
        static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

    VkDescriptorSetLayout descriptorSetLayoutCB = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutCB)
           == VK_SUCCESS);

    return MaterialResources{
        staging,
        default_diffuse,
        default_diffuse_view,
        bricks_diffuse,
        bricks_diffuse_view,
        bricks_roughness,
        bricks_roughness_view,
        sampler,
        descriptorSetLayoutCB,
        create_material_descriptor_set(root, backend, descriptorSetLayoutCB),
    };
}

void destroy_material_resources(VulkanBackend& backend, const MaterialResources& resources)
{
    vkDestroyImageView(backend.device, resources.default_diffuse_view, nullptr);
    vmaDestroyImage(backend.vma_instance, resources.default_diffuse.handle, resources.default_diffuse.allocation);

    vkDestroyImageView(backend.device, resources.bricks_diffuse_view, nullptr);
    vmaDestroyImage(backend.vma_instance, resources.bricks_diffuse.handle, resources.bricks_diffuse.allocation);
    vkDestroyImageView(backend.device, resources.bricks_roughness_view, nullptr);
    vmaDestroyImage(backend.vma_instance, resources.bricks_roughness.handle, resources.bricks_roughness.allocation);

    vkDestroySampler(backend.device, resources.diffuseMapSampler, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.descSetLayout, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.staging.staging_buffer.buffer,
                     resources.staging.staging_buffer.allocation);
}

void update_material_descriptor_set(VulkanBackend& backend, const MaterialResources& resources)
{
    const VkDescriptorImageInfo diffuseMapSampler = {resources.diffuseMapSampler, VK_NULL_HANDLE,
                                                     VK_IMAGE_LAYOUT_UNDEFINED};

    std::vector<VkDescriptorImageInfo> diffuseMaps;
    diffuseMaps.push_back({VK_NULL_HANDLE, resources.default_diffuse_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    diffuseMaps.push_back({VK_NULL_HANDLE, resources.bricks_diffuse_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    diffuseMaps.push_back({VK_NULL_HANDLE, resources.bricks_roughness_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

    const VkWriteDescriptorSet diffuseMapBindlessImageWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        resources.descriptor_set,
        1,
        0,
        static_cast<u32>(diffuseMaps.size()),
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        diffuseMaps.data(),
        nullptr,
        nullptr,
    };

    std::vector<VkWriteDescriptorSet> writes = {
        create_image_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_SAMPLER, &diffuseMapSampler),
        diffuseMapBindlessImageWrite};

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}
} // namespace Reaper
