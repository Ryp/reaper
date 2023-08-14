////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Barrier.h"

#include "Image.h"

#include <core/Assert.h>

namespace Reaper
{
VkImageMemoryBarrier2 get_vk_image_barrier(VkImage handle, const GPUTextureView& view, GPUResourceAccess src,
                                           GPUResourceAccess dst, u32 src_queue_family_index,
                                           u32 dst_queue_family_index)
{
    const VkImageSubresourceRange view_range = GetVulkanImageSubresourceRange(view);

    return VkImageMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                 .pNext = nullptr,
                                 .srcStageMask = src.stage_mask,
                                 .srcAccessMask = src.access_mask,
                                 .dstStageMask = dst.stage_mask,
                                 .dstAccessMask = dst.access_mask,
                                 .oldLayout = src.image_layout,
                                 .newLayout = dst.image_layout,
                                 .srcQueueFamilyIndex = src_queue_family_index,
                                 .dstQueueFamilyIndex = dst_queue_family_index,
                                 .image = handle,
                                 .subresourceRange = view_range};
}

VkBufferMemoryBarrier2 get_vk_buffer_barrier(VkBuffer handle, const GPUBufferView& view, GPUResourceAccess src,
                                             GPUResourceAccess dst, u32 src_queue_family_index,
                                             u32 dst_queue_family_index)
{
    Assert(src.image_layout == VK_IMAGE_LAYOUT_UNDEFINED, "Image layout should be left unused for buffers");
    Assert(dst.image_layout == VK_IMAGE_LAYOUT_UNDEFINED, "Image layout should be left unused for buffers");
    Assert(view.size_bytes > 0, "Invalid view size");

    return VkBufferMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                  .pNext = nullptr,
                                  .srcStageMask = src.stage_mask,
                                  .srcAccessMask = src.access_mask,
                                  .dstStageMask = dst.stage_mask,
                                  .dstAccessMask = dst.access_mask,
                                  .srcQueueFamilyIndex = src_queue_family_index,
                                  .dstQueueFamilyIndex = dst_queue_family_index,
                                  .buffer = handle,
                                  .offset = view.offset_bytes,
                                  .size = view.size_bytes};
}

VkMemoryBarrier2 get_vk_memory_barrier(GPUMemoryAccess src, GPUMemoryAccess dst)
{
    return VkMemoryBarrier2{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = src.stage_mask,
        .srcAccessMask = src.access_mask,
        .dstStageMask = dst.stage_mask,
        .dstAccessMask = dst.access_mask,
    };
}

VkDependencyInfo get_vk_image_barrier_depency_info(u32 barrier_count, const VkImageMemoryBarrier2* barriers)
{
    Assert(barrier_count > 0);
    Assert(barriers != nullptr);

    return VkDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                            .pNext = nullptr,
                            .dependencyFlags = VK_FLAGS_NONE,
                            .memoryBarrierCount = 0,
                            .pMemoryBarriers = nullptr,
                            .bufferMemoryBarrierCount = 0,
                            .pBufferMemoryBarriers = nullptr,
                            .imageMemoryBarrierCount = barrier_count,
                            .pImageMemoryBarriers = barriers};
}

VkDependencyInfo get_vk_buffer_barrier_depency_info(u32 barrier_count, const VkBufferMemoryBarrier2* barriers)
{
    Assert(barrier_count > 0);
    Assert(barriers != nullptr);

    return VkDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                            .pNext = nullptr,
                            .dependencyFlags = VK_FLAGS_NONE,
                            .memoryBarrierCount = 0,
                            .pMemoryBarriers = nullptr,
                            .bufferMemoryBarrierCount = barrier_count,
                            .pBufferMemoryBarriers = barriers,
                            .imageMemoryBarrierCount = 0,
                            .pImageMemoryBarriers = nullptr};
}

VkDependencyInfo get_vk_memory_barrier_depency_info(u32 barrier_count, const VkMemoryBarrier2* barriers)
{
    Assert(barrier_count > 0);
    Assert(barriers != nullptr);

    return VkDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                            .pNext = nullptr,
                            .dependencyFlags = VK_FLAGS_NONE,
                            .memoryBarrierCount = barrier_count,
                            .pMemoryBarriers = barriers,
                            .bufferMemoryBarrierCount = 0,
                            .pBufferMemoryBarriers = nullptr,
                            .imageMemoryBarrierCount = 0,
                            .pImageMemoryBarriers = nullptr};
}

} // namespace Reaper
