// Swapchain readback to a file on disk.
//
// This is what makes a milestone gate checkable without a human looking at the
// window: run with --frame-count N --screenshot <path> and inspect the result.
// The C++ side has no equivalent; --screenshot-frame is listed as optional
// tooling in the port plan.
//
// The copy is recorded into the frame's own command buffer, between rendering
// and the transition back to PRESENT_SRC, because the image may only be
// touched while the frame still owns it.
//
// Output is binary PPM (P6) because it needs no encoder. PNG via lodepng can
// replace it once that translation unit is compiled in.

const std = @import("std");
const vk = @import("vulkan");
const log = std.log.scoped(.vulkan);

const barrier = @import("barrier.zig");

pub const access_copy = barrier.GPUTextureAccess{
    .stage_mask = .{ .copy_bit = true },
    .access_mask = .{ .transfer_read_bit = true },
    .image_layout = .transfer_src_optimal,
};

/// Host-visible staging buffer sized for one full swapchain image.
pub const Readback = struct {
    buffer: vk.Buffer,
    memory: vk.DeviceMemory,
    byte_count: u64,
    extent: vk.Extent2D,
    format: vk.Format,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        memory_properties: vk.PhysicalDeviceMemoryProperties,
        extent: vk.Extent2D,
        format: vk.Format,
    ) !Readback {
        const byte_count = @as(u64, extent.width) * @as(u64, extent.height) * 4;

        const buffer_create_info = vk.BufferCreateInfo{
            .s_type = .buffer_create_info,
            .p_next = null,
            .flags = .{},
            .size = byte_count,
            .usage = .{ .transfer_dst_bit = true },
            .sharing_mode = .exclusive,
            .queue_family_index_count = 0,
            .p_queue_family_indices = null,
        };

        const buffer = try vkd.createBuffer(device, &buffer_create_info, null);
        errdefer vkd.destroyBuffer(device, buffer, null);

        const mem_requirements = vkd.getBufferMemoryRequirements(device, buffer);

        const memory_type_index = findMemoryType(
            memory_properties,
            mem_requirements.memory_type_bits,
            .{ .host_visible_bit = true, .host_coherent_bit = true },
        ) orelse return error.NoHostVisibleMemory;

        const alloc_info = vk.MemoryAllocateInfo{
            .s_type = .memory_allocate_info,
            .p_next = null,
            .allocation_size = mem_requirements.size,
            .memory_type_index = memory_type_index,
        };

        const memory = try vkd.allocateMemory(device, &alloc_info, null);
        errdefer vkd.freeMemory(device, memory, null);

        try vkd.bindBufferMemory(device, buffer, memory, 0);

        return .{
            .buffer = buffer,
            .memory = memory,
            .byte_count = byte_count,
            .extent = extent,
            .format = format,
        };
    }

    pub fn deinit(self: *Readback, vkd: anytype, device: vk.Device) void {
        vkd.freeMemory(device, self.memory, null);
        vkd.destroyBuffer(device, self.buffer, null);
    }
};

/// Records the image→buffer copy. The caller is responsible for having the
/// image in `access_copy`'s layout beforehand.
pub fn recordCopy(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    image: vk.Image,
    readback: *const Readback,
) void {
    const subresource = barrier.defaultTextureSubresourceOneColorMip();

    const regions = [_]vk.BufferImageCopy{.{
        .buffer_offset = 0,
        .buffer_row_length = 0,
        .buffer_image_height = 0,
        .image_subresource = .{
            .aspect_mask = subresource.aspect,
            .mip_level = subresource.mip_offset,
            .base_array_layer = subresource.layer_offset,
            .layer_count = subresource.layer_count,
        },
        .image_offset = .{ .x = 0, .y = 0, .z = 0 },
        .image_extent = .{ .width = readback.extent.width, .height = readback.extent.height, .depth = 1 },
    }};

    vkd.cmdCopyImageToBuffer(cmd_buffer, image, access_copy.image_layout, readback.buffer, &regions);
}

/// Maps the staging buffer and writes it out. Only valid once the submission
/// that recorded the copy has completed.
pub fn write(
    vkd: anytype,
    device: vk.Device,
    readback: *const Readback,
    allocator: std.mem.Allocator,
    io: std.Io,
    path: []const u8,
) !void {
    const mapped = try vkd.mapMemory(device, readback.memory, 0, readback.byte_count, .{});
    defer vkd.unmapMemory(device, readback.memory);

    const pixels: [*]const u8 = @ptrCast(mapped.?);

    try writePpm(
        allocator,
        io,
        path,
        pixels[0..@intCast(readback.byte_count)],
        readback.extent,
        readback.format,
    );

    log.info("wrote screenshot to '{s}' ({}x{})", .{ path, readback.extent.width, readback.extent.height });
}

fn findMemoryType(
    memory_properties: vk.PhysicalDeviceMemoryProperties,
    type_bits: u32,
    required: vk.MemoryPropertyFlags,
) ?u32 {
    for (0..memory_properties.memory_type_count) |i| {
        const index: u5 = @intCast(i);
        if ((type_bits & (@as(u32, 1) << index)) == 0) continue;
        if (!memory_properties.memory_types[i].property_flags.contains(required)) continue;
        return index;
    }
    return null;
}

/// Writes 8-bit RGB in binary PPM. Handles the BGRA channel order that most
/// swapchains hand out.
fn writePpm(
    allocator: std.mem.Allocator,
    io: std.Io,
    path: []const u8,
    pixels: []const u8,
    extent: vk.Extent2D,
    format: vk.Format,
) !void {
    const swap_rb = switch (format) {
        .b8g8r8a8_unorm, .b8g8r8a8_srgb, .b8g8r8a8_snorm => true,
        else => false,
    };

    var out: std.ArrayList(u8) = .empty;
    defer out.deinit(allocator);

    try out.print(allocator, "P6\n{d} {d}\n255\n", .{ extent.width, extent.height });
    try out.ensureUnusedCapacity(allocator, pixels.len / 4 * 3);

    var i: usize = 0;
    while (i + 4 <= pixels.len) : (i += 4) {
        const r = if (swap_rb) pixels[i + 2] else pixels[i];
        const g = pixels[i + 1];
        const b = if (swap_rb) pixels[i] else pixels[i + 2];
        out.appendSliceAssumeCapacity(&[_]u8{ r, g, b });
    }

    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = out.items });
}
