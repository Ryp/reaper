////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

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

struct GPUMemoryAccess
{
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2        access_mask;
};

VkDependencyInfo get_vk_image_barrier_depency_info(u32 barrier_count, const VkImageMemoryBarrier2* barriers);
VkDependencyInfo get_vk_memory_barrier_depency_info(u32 barrier_count, const VkMemoryBarrier2* barriers);

VkImageMemoryBarrier2 get_vk_image_barrier(VkImage handle, GPUTextureView view, GPUTextureAccess src,
                                           GPUTextureAccess dst);
VkMemoryBarrier2      get_vk_memory_barrier(GPUMemoryAccess src, GPUMemoryAccess dst);
} // namespace Reaper
