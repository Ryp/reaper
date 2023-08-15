////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/buffer/GPUBufferView.h"
#include "renderer/texture/GPUTextureView.h"
#include <vulkan_loader/Vulkan.h>

namespace Reaper
{
struct GPUTextureAccess
{
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2        access_mask;
    VkImageLayout         image_layout;
};

struct GPUBufferAccess
{
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2        access_mask;
};

struct GPUMemoryAccess
{
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2        access_mask;
};

VkImageMemoryBarrier2 get_vk_image_barrier(VkImage handle, const GPUTextureView& view, GPUTextureAccess src,
                                           GPUTextureAccess dst, u32 src_queue_family_index = VK_QUEUE_FAMILY_IGNORED,
                                           u32 dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED);

VkBufferMemoryBarrier2 get_vk_buffer_barrier(VkBuffer handle, const GPUBufferView& view, GPUBufferAccess src,
                                             GPUBufferAccess dst, u32 src_queue_family_index = VK_QUEUE_FAMILY_IGNORED,
                                             u32 dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED);

VkMemoryBarrier2 get_vk_memory_barrier(GPUMemoryAccess src, GPUMemoryAccess dst);

VkDependencyInfo get_vk_image_barrier_depency_info(u32 barrier_count, const VkImageMemoryBarrier2* barriers);
VkDependencyInfo get_vk_buffer_barrier_depency_info(u32 barrier_count, const VkBufferMemoryBarrier2* barriers);
VkDependencyInfo get_vk_memory_barrier_depency_info(u32 barrier_count, const VkMemoryBarrier2* barriers);
} // namespace Reaper
