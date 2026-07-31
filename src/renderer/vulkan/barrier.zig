// Port of src/renderer/vulkan/Barrier.h + Barrier.cpp
//
// Only the image-barrier half is needed so far; buffer and memory barriers
// arrive with the frame graph.

const vk = @import("vulkan");

pub const GPUTextureAccess = struct {
    stage_mask: vk.PipelineStageFlags2,
    access_mask: vk.AccessFlags2,
    image_layout: vk.ImageLayout,
};

pub const GPUBufferAccess = struct {
    stage_mask: vk.PipelineStageFlags2,
    access_mask: vk.AccessFlags2,
};

pub const GPUMemoryAccess = struct {
    stage_mask: vk.PipelineStageFlags2,
    access_mask: vk.AccessFlags2,
};

pub const GPUTextureSubresource = struct {
    aspect: vk.ImageAspectFlags,
    mip_offset: u32,
    mip_count: u32,
    layer_offset: u32,
    layer_count: u32,
};

/// Mirrors default_texture_subresource_one_color_mip().
pub fn defaultTextureSubresourceOneColorMip() GPUTextureSubresource {
    return .{
        .aspect = .{ .color_bit = true },
        .mip_offset = 0,
        .mip_count = 1,
        .layer_offset = 0,
        .layer_count = 1,
    };
}

pub fn getImageBarrier(
    handle: vk.Image,
    subresource: GPUTextureSubresource,
    src: GPUTextureAccess,
    dst: GPUTextureAccess,
    src_queue_family_index: u32,
    dst_queue_family_index: u32,
) vk.ImageMemoryBarrier2 {
    return .{
        .s_type = .image_memory_barrier_2,
        .p_next = null,
        .src_stage_mask = src.stage_mask,
        .src_access_mask = src.access_mask,
        .dst_stage_mask = dst.stage_mask,
        .dst_access_mask = dst.access_mask,
        .old_layout = src.image_layout,
        .new_layout = dst.image_layout,
        .src_queue_family_index = src_queue_family_index,
        .dst_queue_family_index = dst_queue_family_index,
        .image = handle,
        .subresource_range = .{
            .aspect_mask = subresource.aspect,
            .base_mip_level = subresource.mip_offset,
            .level_count = subresource.mip_count,
            .base_array_layer = subresource.layer_offset,
            .layer_count = subresource.layer_count,
        },
    };
}

/// Same as getImageBarrier() with both queue family indices ignored, which is
/// the default in the C++ signature.
pub fn getImageBarrierSameQueue(
    handle: vk.Image,
    subresource: GPUTextureSubresource,
    src: GPUTextureAccess,
    dst: GPUTextureAccess,
) vk.ImageMemoryBarrier2 {
    return getImageBarrier(
        handle,
        subresource,
        src,
        dst,
        vk.QUEUE_FAMILY_IGNORED,
        vk.QUEUE_FAMILY_IGNORED,
    );
}

pub fn getImageBarrierDependencyInfo(barriers: []const vk.ImageMemoryBarrier2) vk.DependencyInfo {
    return .{
        .s_type = .dependency_info,
        .p_next = null,
        .dependency_flags = .{},
        .memory_barrier_count = 0,
        .p_memory_barriers = null,
        .buffer_memory_barrier_count = 0,
        .p_buffer_memory_barriers = null,
        .image_memory_barrier_count = @intCast(barriers.len),
        .p_image_memory_barriers = barriers.ptr,
    };
}
