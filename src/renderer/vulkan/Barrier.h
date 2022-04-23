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

VkDependencyInfo get_vk_image_barrier_depency_info(u32 barrier_count, const VkImageMemoryBarrier2* barriers);

VkImageMemoryBarrier2 get_vk_image_barrier(VkImage handle, GPUTextureView view, GPUTextureAccess src,
                                           GPUTextureAccess dst);
} // namespace Reaper
