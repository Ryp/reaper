////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Barrier.h"

#include "Image.h"

namespace Reaper
{
VkImageMemoryBarrier2 get_vk_image_barrier(VkImage handle, const GPUTextureView& view, GPUTextureAccess src,
                                           GPUTextureAccess dst)
{
    const VkImageSubresourceRange view_range = GetVulkanImageSubresourceRange(view);

    return VkImageMemoryBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                 nullptr,
                                 src.stage_mask,
                                 src.access_mask,
                                 dst.stage_mask,
                                 dst.access_mask,
                                 src.layout,
                                 dst.layout,
                                 src.queueFamilyIndex,
                                 dst.queueFamilyIndex,
                                 handle,
                                 view_range};
}

VkBufferMemoryBarrier2 get_vk_buffer_barrier(VkBuffer handle, const GPUBufferView& view, GPUBufferAccess src,
                                             GPUBufferAccess dst)
{
    return VkBufferMemoryBarrier2{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        nullptr,
        src.stage_mask,
        src.access_mask,
        dst.stage_mask,
        dst.access_mask,
        src.queueFamilyIndex,
        dst.queueFamilyIndex,
        handle,
        view.elementOffset,
        VK_WHOLE_SIZE // FIXME
    };
}

VkMemoryBarrier2 get_vk_memory_barrier(GPUMemoryAccess src, GPUMemoryAccess dst)
{
    return VkMemoryBarrier2{
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, nullptr, src.stage_mask, src.access_mask, dst.stage_mask, dst.access_mask,
    };
}

VkDependencyInfo get_vk_image_barrier_depency_info(u32 barrier_count, const VkImageMemoryBarrier2* barriers)
{
    Assert(barrier_count > 0);
    Assert(barriers != nullptr);

    return VkDependencyInfo{
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 0, nullptr, barrier_count, barriers};
}

VkDependencyInfo get_vk_buffer_barrier_depency_info(u32 barrier_count, const VkBufferMemoryBarrier2* barriers)
{
    Assert(barrier_count > 0);
    Assert(barriers != nullptr);

    return VkDependencyInfo{
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, barrier_count, barriers, 0, nullptr};
}

VkDependencyInfo get_vk_memory_barrier_depency_info(u32 barrier_count, const VkMemoryBarrier2* barriers)
{
    Assert(barrier_count > 0);
    Assert(barriers != nullptr);

    return VkDependencyInfo{
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, VK_FLAGS_NONE, barrier_count, barriers, 0, nullptr, 0, nullptr};
}

} // namespace Reaper
