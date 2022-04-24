////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/GPUBufferView.h"
#include "renderer/texture/GPUTextureView.h"
#include "renderer/vulkan/api/Vulkan.h"

namespace Reaper
{
struct GPUTextureAccess
{
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2        access_mask;
    VkImageLayout         layout;
    u32                   queueFamilyIndex;
};

struct GPUBufferAccess
{
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2        access_mask;
    u32                   queueFamilyIndex;
};

struct GPUMemoryAccess
{
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2        access_mask;
};

VkImageMemoryBarrier2  get_vk_image_barrier(VkImage handle, const GPUTextureView& view, GPUTextureAccess src,
                                            GPUTextureAccess dst);
VkBufferMemoryBarrier2 get_vk_buffer_barrier(VkBuffer handle, const GPUBufferView& view, GPUBufferAccess src,
                                             GPUBufferAccess dst);
VkMemoryBarrier2       get_vk_memory_barrier(GPUMemoryAccess src, GPUMemoryAccess dst);

VkDependencyInfo get_vk_image_barrier_depency_info(u32 barrier_count, const VkImageMemoryBarrier2* barriers);
VkDependencyInfo get_vk_buffer_barrier_depency_info(u32 barrier_count, const VkBufferMemoryBarrier2* barriers);
VkDependencyInfo get_vk_memory_barrier_depency_info(u32 barrier_count, const VkMemoryBarrier2* barriers);
} // namespace Reaper
