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
// Output is PNG, encoded with the same lodepng the texture loader uses. The
// encode goes to memory rather than through lodepng's own file I/O so the
// write still goes through the caller's std.Io.

const std = @import("std");
const vk = @import("vulkan");
const log = std.log.scoped(.vulkan);

const barrier = @import("barrier.zig");
const lodepng = @import("lodepng.zig").c;

pub const access_copy = barrier.GPUTextureAccess{
    .stage_mask = .{ .copy_bit = true },
    .access_mask = .{ .transfer_read_bit = true },
    .image_layout = .transfer_src_optimal,
};

/// How to get 8-bit RGB out of one swapchain texel. The swapchain format is
/// whatever chooseSwapchainFormat landed on, which on a Wayland surface is a
/// 64-bit format rather than the 32-bit one an X11 surface offers — so this
/// cannot assume 4 bytes per pixel.
const PixelLayout = struct {
    bytes_per_pixel: u32,
    decode: *const fn (texel: []const u8) [3]u8,

    fn get(format: vk.Format) ?PixelLayout {
        return switch (format) {
            .r8g8b8a8_unorm, .r8g8b8a8_srgb, .r8g8b8a8_snorm => .{ .bytes_per_pixel = 4, .decode = decodeRgba8 },
            .b8g8r8a8_unorm, .b8g8r8a8_srgb, .b8g8r8a8_snorm => .{ .bytes_per_pixel = 4, .decode = decodeBgra8 },
            .r16g16b16a16_unorm => .{ .bytes_per_pixel = 8, .decode = decodeRgba16Unorm },
            .r16g16b16a16_sfloat => .{ .bytes_per_pixel = 8, .decode = decodeRgba16Sfloat },
            .a2b10g10r10_unorm_pack32 => .{ .bytes_per_pixel = 4, .decode = decodeA2Bgr10 },
            .a2r10g10b10_unorm_pack32 => .{ .bytes_per_pixel = 4, .decode = decodeA2Rgb10 },
            .r5g6b5_unorm_pack16 => .{ .bytes_per_pixel = 2, .decode = decodeRgb565 },
            else => null,
        };
    }
};

fn decodeRgba8(texel: []const u8) [3]u8 {
    return .{ texel[0], texel[1], texel[2] };
}

fn decodeBgra8(texel: []const u8) [3]u8 {
    return .{ texel[2], texel[1], texel[0] };
}

fn decodeRgba16Unorm(texel: []const u8) [3]u8 {
    // Truncating to the high byte is exactly the 16→8 bit reduction PPM needs.
    return .{ texel[1], texel[3], texel[5] };
}

fn decodeRgba16Sfloat(texel: []const u8) [3]u8 {
    var out: [3]u8 = undefined;
    for (&out, 0..) |*channel, i| {
        const half: f16 = @bitCast(std.mem.readInt(u16, texel[i * 2 ..][0..2], .little));
        const value = std.math.clamp(@as(f32, half), 0.0, 1.0);
        channel.* = @intFromFloat(@round(value * 255.0));
    }
    return out;
}

fn decodeA2Bgr10(texel: []const u8) [3]u8 {
    const packed_texel = std.mem.readInt(u32, texel[0..4], .little);
    return .{
        @intCast((packed_texel >> 2) & 0xFF),
        @intCast((packed_texel >> 12) & 0xFF),
        @intCast((packed_texel >> 22) & 0xFF),
    };
}

fn decodeA2Rgb10(texel: []const u8) [3]u8 {
    const packed_texel = std.mem.readInt(u32, texel[0..4], .little);
    return .{
        @intCast((packed_texel >> 22) & 0xFF),
        @intCast((packed_texel >> 12) & 0xFF),
        @intCast((packed_texel >> 2) & 0xFF),
    };
}

fn decodeRgb565(texel: []const u8) [3]u8 {
    const packed_texel = std.mem.readInt(u16, texel[0..2], .little);
    // Replicate the high bits into the low ones so full-scale stays full-scale.
    const r5: u8 = @intCast((packed_texel >> 11) & 0x1F);
    const g6: u8 = @intCast((packed_texel >> 5) & 0x3F);
    const b5: u8 = @intCast(packed_texel & 0x1F);
    return .{ (r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2) };
}

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
        const layout = PixelLayout.get(format) orelse return error.UnsupportedScreenshotFormat;
        const byte_count = @as(u64, extent.width) * @as(u64, extent.height) * layout.bytes_per_pixel;

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

    try writePng(
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

/// Converts the swapchain's texels to 8-bit RGB and writes them out as PNG.
fn writePng(
    allocator: std.mem.Allocator,
    io: std.Io,
    path: []const u8,
    pixels: []const u8,
    extent: vk.Extent2D,
    format: vk.Format,
) !void {
    const rgb = try decodeToRgb(allocator, pixels, extent, format);
    defer allocator.free(rgb);

    // lodepng allocates the encoded buffer with its own allocator, so it has to
    // come back through free() rather than the Zig allocator above.
    var encoded_ptr: [*c]u8 = null;
    var encoded_len: usize = 0;

    const err = lodepng.lodepng_encode24(
        &encoded_ptr,
        &encoded_len,
        rgb.ptr,
        extent.width,
        extent.height,
    );
    defer std.c.free(encoded_ptr);

    if (err != 0) {
        log.err("lodepng error {} encoding '{s}': {s}", .{ err, path, lodepng.lodepng_error_text(err) });
        return error.PngEncodeFailed;
    }

    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = encoded_ptr[0..encoded_len] });
}

/// Unpacks whatever the swapchain format is into tightly packed 8-bit RGB,
/// which is what both the PNG encoder and the layout tests work in.
fn decodeToRgb(
    allocator: std.mem.Allocator,
    pixels: []const u8,
    extent: vk.Extent2D,
    format: vk.Format,
) ![]u8 {
    const layout = PixelLayout.get(format) orelse return error.UnsupportedScreenshotFormat;
    const stride = layout.bytes_per_pixel;
    const texel_count = @as(usize, extent.width) * @as(usize, extent.height);

    if (pixels.len < texel_count * stride) return error.ScreenshotBufferTooSmall;

    const rgb = try allocator.alloc(u8, texel_count * 3);
    errdefer allocator.free(rgb);

    for (0..texel_count) |i| {
        rgb[i * 3 ..][0..3].* = layout.decode(pixels[i * stride ..][0..stride]);
    }

    return rgb;
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "every format the swapchain chooser can pick has a decoder" {
    // These are the nine VK_COLOR_SPACE_SRGB_NONLINEAR_KHR formats a Wayland
    // surface offers on RADV; the chooser can land on any of them, and a
    // missing decoder would only show up as a failed screenshot at gate time.
    const selectable = [_]vk.Format{
        .r16g16b16a16_sfloat,
        .r16g16b16a16_unorm,
        .a2r10g10b10_unorm_pack32,
        .a2b10g10r10_unorm_pack32,
        .b8g8r8a8_srgb,
        .b8g8r8a8_unorm,
        .r8g8b8a8_srgb,
        .r8g8b8a8_unorm,
        .r5g6b5_unorm_pack16,
    };

    for (selectable) |format| {
        try testing.expect(PixelLayout.get(format) != null);
    }
}

test "decoders agree on a mid-grey texel" {
    // Same colour expressed in each format; the decoders must not disagree on
    // channel order, which is the failure mode that turns a screenshot into a
    // red/blue-swapped image nobody notices.
    try testing.expectEqual([3]u8{ 0x11, 0x22, 0x33 }, decodeRgba8(&.{ 0x11, 0x22, 0x33, 0xFF }));
    try testing.expectEqual([3]u8{ 0x11, 0x22, 0x33 }, decodeBgra8(&.{ 0x33, 0x22, 0x11, 0xFF }));
    try testing.expectEqual(
        [3]u8{ 0x11, 0x22, 0x33 },
        decodeRgba16Unorm(&.{ 0x00, 0x11, 0x00, 0x22, 0x00, 0x33, 0xFF, 0xFF }),
    );
}

test "full scale stays full scale through every decoder" {
    // Truncation that loses the top of the range would darken every
    // screenshot slightly — exactly the kind of drift an eyeball gate misses.
    try testing.expectEqual([3]u8{ 255, 255, 255 }, decodeRgba8(&.{ 255, 255, 255, 255 }));
    try testing.expectEqual([3]u8{ 255, 255, 255 }, decodeRgba16Unorm(&(.{0xFF} ** 8)));
    try testing.expectEqual([3]u8{ 255, 255, 255 }, decodeA2Bgr10(&.{ 0xFF, 0xFF, 0xFF, 0xFF }));
    try testing.expectEqual([3]u8{ 255, 255, 255 }, decodeA2Rgb10(&.{ 0xFF, 0xFF, 0xFF, 0xFF }));
    try testing.expectEqual([3]u8{ 255, 255, 255 }, decodeRgb565(&.{ 0xFF, 0xFF }));

    var white_half: [8]u8 = undefined;
    for (0..4) |i| std.mem.writeInt(u16, white_half[i * 2 ..][0..2], @bitCast(@as(f16, 1.0)), .little);
    try testing.expectEqual([3]u8{ 255, 255, 255 }, decodeRgba16Sfloat(&white_half));
}

test "decodeToRgb packs tightly and drops the alpha channel" {
    // Two BGRA texels; the output must be 6 bytes, not 8, or the PNG encoder
    // reads past the end of the row and the image shears.
    const texels = [_]u8{ 0x33, 0x22, 0x11, 0xFF, 0x66, 0x55, 0x44, 0xFF };

    const rgb = try decodeToRgb(testing.allocator, &texels, .{ .width = 2, .height = 1 }, .b8g8r8a8_unorm);
    defer testing.allocator.free(rgb);

    try testing.expectEqualSlices(u8, &.{ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 }, rgb);
}

test "decodeToRgb rejects a buffer that cannot hold the extent" {
    // The readback buffer is sized from the swapchain format, so a mismatch
    // here means the two disagree — worth an error rather than a garbage image.
    try testing.expectError(
        error.ScreenshotBufferTooSmall,
        decodeToRgb(testing.allocator, &[_]u8{0} ** 4, .{ .width = 2, .height = 1 }, .b8g8r8a8_unorm),
    );
}

test "sfloat clamps out-of-range HDR values instead of wrapping" {
    var texel: [8]u8 = undefined;
    std.mem.writeInt(u16, texel[0..2], @bitCast(@as(f16, 4.0)), .little);
    std.mem.writeInt(u16, texel[2..4], @bitCast(@as(f16, -1.0)), .little);
    std.mem.writeInt(u16, texel[4..6], @bitCast(@as(f16, 0.5)), .little);
    std.mem.writeInt(u16, texel[6..8], @bitCast(@as(f16, 1.0)), .little);

    try testing.expectEqual([3]u8{ 255, 0, 128 }, decodeRgba16Sfloat(&texel));
}
