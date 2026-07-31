// Port of src/renderer/vulkan/renderpass/LightingPass.{h,cpp}
//
// Just an allocation out of the frame storage allocator: the point light array
// is uploaded once per frame and read by both the forward and tiled paths.

const std = @import("std");

const hlsl_lighting = @import("../../hlsl/lighting.zig");
const prepare_buckets = @import("../../prepare_buckets.zig");
const storage_buffer = @import("../storage_buffer.zig");

pub const LightingPassResources = struct {
    point_light_buffer_alloc: storage_buffer.StorageBufferAlloc = .{
        .buffer = .null_handle,
        .offset_bytes = 0,
        .size_bytes = 0,
    },
};

pub fn uploadFrameResources(
    frame_storage_allocator: *storage_buffer.StorageBufferAllocator,
    prepared: *const prepare_buckets.PreparedData,
    resources: *LightingPassResources,
) void {
    if (prepared.point_lights.items.len == 0) return;

    resources.point_light_buffer_alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_lighting.PointLightProperties,
        prepared.point_lights.items,
    );
}
