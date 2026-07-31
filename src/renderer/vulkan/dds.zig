// DDS parser — replaces the vendored tinyddsloader.h for the Zig build.
//
// Scope matches what the C++ actually consumes: DX10-header files, 2D, one
// array layer. The format table is exactly the set `get_dds_pixel_format` in
// TextureLoadingDDS.cpp maps; anything else made the C++ hit AssertUnreachable,
// so it is an error here rather than a silent fallback.
//
// Mip sizes and the per-image walk follow tinyddsloader's GetImageInfo/Load so
// the byte offsets handed to the staging copy are identical.

const std = @import("std");
const vk = @import("vulkan");

const magic: u32 = std.mem.readInt(u32, "DDS ", .little);
const four_cc_dx10: u32 = std.mem.readInt(u32, "DX10", .little);

const header_flag_volume: u32 = 0x00800000;
const pixel_format_flag_four_cc: u32 = 0x4;
const dxt10_misc_texture_cube: u32 = 0x4;

const PixelFormatHeader = extern struct {
    size: u32,
    flags: u32,
    four_cc: u32,
    rgb_bit_count: u32,
    r_bit_mask: u32,
    g_bit_mask: u32,
    b_bit_mask: u32,
    a_bit_mask: u32,
};

const Header = extern struct {
    size: u32,
    flags: u32,
    height: u32,
    width: u32,
    pitch_or_linear_size: u32,
    depth: u32,
    mip_map_count: u32,
    reserved1: [11]u32,
    pixel_format: PixelFormatHeader,
    caps: u32,
    caps2: u32,
    caps3: u32,
    caps4: u32,
    reserved2: u32,
};

const HeaderDxt10 = extern struct {
    dxgi_format: u32,
    resource_dimension: u32,
    misc_flag: u32,
    array_size: u32,
    misc_flags2: u32,
};

comptime {
    std.debug.assert(@sizeOf(PixelFormatHeader) == 32);
    std.debug.assert(@sizeOf(Header) == 124);
    std.debug.assert(@sizeOf(HeaderDxt10) == 20);
}

const ResourceDimension = enum(u32) {
    unknown = 0,
    buffer = 1,
    texture_1d = 2,
    texture_2d = 3,
    texture_3d = 4,
    _,
};

/// The subset of DXGI_FORMAT the engine maps to a Vulkan format.
const FormatInfo = struct {
    vk_format: vk.Format,
    /// Block-compressed formats size their mips in 4x4 blocks.
    block_compressed: bool,
    /// Bytes per block for BC, bytes per pixel otherwise.
    bytes_per_element: u32,
};

fn formatInfo(dxgi_format: u32) ?FormatInfo {
    // FIXME BC1 is assumed to carry no alpha, same as the C++.
    return switch (dxgi_format) {
        71 => .{ .vk_format = .bc1_rgb_unorm_block, .block_compressed = true, .bytes_per_element = 8 },
        72 => .{ .vk_format = .bc1_rgb_srgb_block, .block_compressed = true, .bytes_per_element = 8 },
        74 => .{ .vk_format = .bc2_unorm_block, .block_compressed = true, .bytes_per_element = 16 },
        75 => .{ .vk_format = .bc2_srgb_block, .block_compressed = true, .bytes_per_element = 16 },
        77 => .{ .vk_format = .bc3_unorm_block, .block_compressed = true, .bytes_per_element = 16 },
        78 => .{ .vk_format = .bc3_srgb_block, .block_compressed = true, .bytes_per_element = 16 },
        80 => .{ .vk_format = .bc4_unorm_block, .block_compressed = true, .bytes_per_element = 8 },
        81 => .{ .vk_format = .bc4_snorm_block, .block_compressed = true, .bytes_per_element = 8 },
        83 => .{ .vk_format = .bc5_unorm_block, .block_compressed = true, .bytes_per_element = 16 },
        84 => .{ .vk_format = .bc5_snorm_block, .block_compressed = true, .bytes_per_element = 16 },
        87 => .{ .vk_format = .b8g8r8a8_unorm, .block_compressed = false, .bytes_per_element = 4 },
        95 => .{ .vk_format = .bc6h_ufloat_block, .block_compressed = true, .bytes_per_element = 16 },
        96 => .{ .vk_format = .bc6h_sfloat_block, .block_compressed = true, .bytes_per_element = 16 },
        98 => .{ .vk_format = .bc7_unorm_block, .block_compressed = true, .bytes_per_element = 16 },
        99 => .{ .vk_format = .bc7_srgb_block, .block_compressed = true, .bytes_per_element = 16 },
        else => null,
    };
}

const ImageInfo = struct { size_bytes: u32, row_pitch: u32 };

fn imageInfo(width: u32, height: u32, info: FormatInfo) ImageInfo {
    if (info.block_compressed) {
        const blocks_wide = if (width > 0) @max(1, (width + 3) / 4) else 0;
        const blocks_high = if (height > 0) @max(1, (height + 3) / 4) else 0;
        const row_pitch = blocks_wide * info.bytes_per_element;

        return .{ .size_bytes = row_pitch * blocks_high, .row_pitch = row_pitch };
    }

    const row_pitch = width * info.bytes_per_element;

    return .{ .size_bytes = row_pitch * height, .row_pitch = row_pitch };
}

pub const Image = struct {
    width: u32,
    height: u32,
    depth: u32,
    row_pitch: u32,
    /// One depth slice. `depth` is always 1 for the 2D files the engine loads.
    slice_pitch: u32,
    /// Borrowed from the caller's file bytes.
    data: []const u8,
};

pub const DdsFile = struct {
    width: u32,
    height: u32,
    depth: u32,
    mip_count: u32,
    array_size: u32,
    format: vk.Format,
    is_cubemap: bool,
    dimension: ResourceDimension,

    /// mip-major within each array layer, as tinyddsloader lays them out.
    images: []Image,

    pub fn deinit(self: *DdsFile, allocator: std.mem.Allocator) void {
        allocator.free(self.images);
        self.* = undefined;
    }

    pub fn imageData(self: *const DdsFile, mip_index: u32, layer_index: u32) *const Image {
        return &self.images[self.mip_count * layer_index + mip_index];
    }
};

/// `bytes` must outlive the returned file: every `Image.data` points into it.
pub fn parse(allocator: std.mem.Allocator, bytes: []const u8) !DdsFile {
    if (bytes.len < @sizeOf(u32) + @sizeOf(Header)) return error.TruncatedDds;
    if (std.mem.readInt(u32, bytes[0..4], .little) != magic) return error.NotADdsFile;

    const header = std.mem.bytesToValue(Header, bytes[4..][0..@sizeOf(Header)]);

    if (header.size != @sizeOf(Header) or header.pixel_format.size != @sizeOf(PixelFormatHeader))
        return error.InvalidDdsHeader;

    const has_dxt10 = (header.pixel_format.flags & pixel_format_flag_four_cc) != 0 and
        header.pixel_format.four_cc == four_cc_dx10;

    // Only DX10-header files are supported — see the file comment.
    if (!has_dxt10) return error.UnsupportedDdsFormat;

    const header_end = @sizeOf(u32) + @sizeOf(Header);
    if (bytes.len <= header_end + @sizeOf(HeaderDxt10)) return error.TruncatedDds;

    const dxt10 = std.mem.bytesToValue(HeaderDxt10, bytes[header_end..][0..@sizeOf(HeaderDxt10)]);

    if (dxt10.array_size == 0) return error.InvalidDdsHeader;

    const info = formatInfo(dxt10.dxgi_format) orelse return error.UnsupportedDdsFormat;

    const width = header.width;
    var height = header.height;
    var depth = header.depth;
    var array_size = dxt10.array_size;
    var is_cubemap = false;

    const dimension: ResourceDimension = @enumFromInt(dxt10.resource_dimension);

    switch (dimension) {
        .texture_1d => {
            height = 1;
            depth = 1;
        },
        .texture_2d => {
            if ((dxt10.misc_flag & dxt10_misc_texture_cube) != 0) {
                array_size *= 6;
                is_cubemap = true;
            }
            depth = 1;
        },
        .texture_3d => {
            if ((header.flags & header_flag_volume) == 0) return error.InvalidDdsHeader;
            if (array_size > 1) return error.UnsupportedDdsFormat;
        },
        else => return error.UnsupportedDdsFormat,
    }

    const mip_count = @max(1, header.mip_map_count);

    const images = try allocator.alloc(Image, mip_count * array_size);
    errdefer allocator.free(images);

    var offset: usize = header_end + @sizeOf(HeaderDxt10);
    var index: usize = 0;

    for (0..array_size) |_| {
        var w = width;
        var h = height;
        var d = depth;

        for (0..mip_count) |_| {
            const mip_info = imageInfo(w, h, info);
            const slice_bytes = @as(usize, mip_info.size_bytes) * d;

            if (offset + slice_bytes > bytes.len) return error.TruncatedDds;

            images[index] = .{
                .width = w,
                .height = h,
                .depth = d,
                .row_pitch = mip_info.row_pitch,
                .slice_pitch = mip_info.size_bytes,
                .data = bytes[offset..][0..slice_bytes],
            };
            index += 1;

            offset += slice_bytes;
            w = @max(1, w / 2);
            h = @max(1, h / 2);
            d = @max(1, d / 2);
        }
    }

    return .{
        .width = width,
        .height = height,
        .depth = depth,
        .mip_count = mip_count,
        .array_size = array_size,
        .format = info.vk_format,
        .is_cubemap = is_cubemap,
        .dimension = dimension,
        .images = images,
    };
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

test "mip sizing matches tinyddsloader for block-compressed formats" {
    const bc7 = FormatInfo{ .vk_format = .bc7_unorm_block, .block_compressed = true, .bytes_per_element = 16 };

    // 2048x2048 BC7 is exactly one byte per pixel.
    try std.testing.expectEqual(@as(u32, 2048 * 2048), imageInfo(2048, 2048, bc7).size_bytes);
    try std.testing.expectEqual(@as(u32, 512 * 16), imageInfo(2048, 2048, bc7).row_pitch);

    // Sub-block mips still cost one full block.
    try std.testing.expectEqual(@as(u32, 16), imageInfo(1, 1, bc7).size_bytes);
    try std.testing.expectEqual(@as(u32, 16), imageInfo(2, 2, bc7).size_bytes);
    try std.testing.expectEqual(@as(u32, 16), imageInfo(4, 4, bc7).size_bytes);
    try std.testing.expectEqual(@as(u32, 4 * 16), imageInfo(5, 5, bc7).size_bytes);

    const bc1 = FormatInfo{ .vk_format = .bc1_rgb_unorm_block, .block_compressed = true, .bytes_per_element = 8 };
    try std.testing.expectEqual(@as(u32, 2048 * 2048 / 2), imageInfo(2048, 2048, bc1).size_bytes);

    const bgra = FormatInfo{ .vk_format = .b8g8r8a8_unorm, .block_compressed = false, .bytes_per_element = 4 };
    try std.testing.expectEqual(@as(u32, 64 * 32 * 4), imageInfo(64, 32, bgra).size_bytes);
}

test "parses the SciFiHelmet DDS textures" {
    const allocator = std.testing.allocator;

    const expected = [_]struct { path: []const u8, format: vk.Format }{
        .{ .path = "res/model/sci_fi_helmet/SciFiHelmet_BaseColor.dds", .format = .bc7_srgb_block },
        .{ .path = "res/model/sci_fi_helmet/SciFiHelmet_MetallicRoughness.dds", .format = .bc7_unorm_block },
        .{ .path = "res/model/sci_fi_helmet/SciFiHelmet_Normal.dds", .format = .bc7_unorm_block },
        .{ .path = "res/model/sci_fi_helmet/SciFiHelmet_AmbientOcclusion.dds", .format = .bc7_unorm_block },
    };

    const io = std.testing.io;

    for (expected) |entry| {
        const bytes = try std.Io.Dir.cwd().readFileAlloc(io, entry.path, allocator, .limited(1 << 30));
        defer allocator.free(bytes);

        var file = try parse(allocator, bytes);
        defer file.deinit(allocator);

        try std.testing.expectEqual(@as(u32, 2048), file.width);
        try std.testing.expectEqual(@as(u32, 2048), file.height);
        try std.testing.expectEqual(@as(u32, 1), file.depth);
        try std.testing.expectEqual(@as(u32, 1), file.mip_count);
        try std.testing.expectEqual(@as(u32, 1), file.array_size);
        try std.testing.expectEqual(entry.format, file.format);
        try std.testing.expect(!file.is_cubemap);
        try std.testing.expectEqual(ResourceDimension.texture_2d, file.dimension);

        const image = file.imageData(0, 0);
        try std.testing.expectEqual(@as(usize, 2048 * 2048), image.data.len);

        // The payload must land right after the 148-byte header chain, and
        // account for every byte of the file.
        try std.testing.expectEqual(bytes.len - 148, image.data.len);
        try std.testing.expectEqual(bytes[148], image.data[0]);
    }
}

/// Builds a DX10 DDS in memory whose payload byte `i` of (layer, mip) is
/// `(layer * 97 + mip * 31 + i) & 0xff`, so the first and last byte of every
/// image identify the offset it was read from.
fn synthesiseBc7Dds(
    allocator: std.mem.Allocator,
    width: u32,
    height: u32,
    mip_count: u32,
    array_size: u32,
) ![]u8 {
    const info = formatInfo(98).?; // BC7_UNorm

    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(allocator);

    try out.appendSlice(allocator, "DDS ");

    var header = std.mem.zeroes(Header);
    header.size = @sizeOf(Header);
    header.flags = 0x000A1007;
    header.width = width;
    header.height = height;
    header.pitch_or_linear_size = imageInfo(width, height, info).size_bytes;
    header.mip_map_count = mip_count;
    header.pixel_format.size = @sizeOf(PixelFormatHeader);
    header.pixel_format.flags = pixel_format_flag_four_cc;
    header.pixel_format.four_cc = four_cc_dx10;
    header.caps = 0x1000;
    try out.appendSlice(allocator, std.mem.asBytes(&header));

    const dxt10 = HeaderDxt10{
        .dxgi_format = 98,
        .resource_dimension = @intFromEnum(ResourceDimension.texture_2d),
        .misc_flag = 0,
        .array_size = array_size,
        .misc_flags2 = 0,
    };
    try out.appendSlice(allocator, std.mem.asBytes(&dxt10));

    for (0..array_size) |layer| {
        var w = width;
        var h = height;

        for (0..mip_count) |mip| {
            const size_bytes = imageInfo(w, h, info).size_bytes;

            for (0..size_bytes) |i| {
                try out.append(allocator, @truncate(layer * 97 + mip * 31 + i));
            }

            w = @max(1, w / 2);
            h = @max(1, h / 2);
        }
    }

    return out.toOwnedSlice(allocator);
}

test "mip chains and array layers match tinyddsloader byte for byte" {
    const allocator = std.testing.allocator;

    // Every expected row below was produced by a probe built against the
    // vendored external/tinyddsloader/tinyddsloader.h on the same inputs.
    const Row = struct { layer: u32, mip: u32, w: u32, h: u32, pitch: u32, slice: u32, first: u8, last: u8 };

    const cases = [_]struct {
        width: u32,
        height: u32,
        mip_count: u32,
        array_size: u32,
        rows: []const Row,
    }{
        .{
            .width = 256,
            .height = 256,
            .mip_count = 9,
            .array_size = 1,
            .rows = &.{
                .{ .layer = 0, .mip = 0, .w = 256, .h = 256, .pitch = 1024, .slice = 65536, .first = 0x00, .last = 0xff },
                .{ .layer = 0, .mip = 1, .w = 128, .h = 128, .pitch = 512, .slice = 16384, .first = 0x1f, .last = 0x1e },
                .{ .layer = 0, .mip = 2, .w = 64, .h = 64, .pitch = 256, .slice = 4096, .first = 0x3e, .last = 0x3d },
                .{ .layer = 0, .mip = 3, .w = 32, .h = 32, .pitch = 128, .slice = 1024, .first = 0x5d, .last = 0x5c },
                .{ .layer = 0, .mip = 4, .w = 16, .h = 16, .pitch = 64, .slice = 256, .first = 0x7c, .last = 0x7b },
                .{ .layer = 0, .mip = 5, .w = 8, .h = 8, .pitch = 32, .slice = 64, .first = 0x9b, .last = 0xda },
                // Below one block the mip keeps costing a full block.
                .{ .layer = 0, .mip = 6, .w = 4, .h = 4, .pitch = 16, .slice = 16, .first = 0xba, .last = 0xc9 },
                .{ .layer = 0, .mip = 7, .w = 2, .h = 2, .pitch = 16, .slice = 16, .first = 0xd9, .last = 0xe8 },
                .{ .layer = 0, .mip = 8, .w = 1, .h = 1, .pitch = 16, .slice = 16, .first = 0xf8, .last = 0x07 },
            },
        },
        // Layers are laid out one whole mip chain after another.
        .{ .width = 64, .height = 32, .mip_count = 7, .array_size = 3, .rows = &.{
            .{ .layer = 0, .mip = 0, .w = 64, .h = 32, .pitch = 256, .slice = 2048, .first = 0x00, .last = 0xff },
            .{ .layer = 0, .mip = 6, .w = 1, .h = 1, .pitch = 16, .slice = 16, .first = 0xba, .last = 0xc9 },
            .{ .layer = 1, .mip = 0, .w = 64, .h = 32, .pitch = 256, .slice = 2048, .first = 0x61, .last = 0x60 },
            .{ .layer = 1, .mip = 3, .w = 8, .h = 4, .pitch = 32, .slice = 32, .first = 0xbe, .last = 0xdd },
            .{ .layer = 2, .mip = 0, .w = 64, .h = 32, .pitch = 256, .slice = 2048, .first = 0xc2, .last = 0xc1 },
            .{ .layer = 2, .mip = 6, .w = 1, .h = 1, .pitch = 16, .slice = 16, .first = 0x7c, .last = 0x8b },
        } },
        // Non-power-of-two: block rounding, and mips that halve unevenly.
        .{ .width = 100, .height = 37, .mip_count = 4, .array_size = 1, .rows = &.{
            .{ .layer = 0, .mip = 0, .w = 100, .h = 37, .pitch = 400, .slice = 4000, .first = 0x00, .last = 0x9f },
            .{ .layer = 0, .mip = 1, .w = 50, .h = 18, .pitch = 208, .slice = 1040, .first = 0x1f, .last = 0x2e },
            .{ .layer = 0, .mip = 2, .w = 25, .h = 9, .pitch = 112, .slice = 336, .first = 0x3e, .last = 0x8d },
            .{ .layer = 0, .mip = 3, .w = 12, .h = 4, .pitch = 48, .slice = 48, .first = 0x5d, .last = 0x8c },
        } },
    };

    for (cases) |case| {
        const bytes = try synthesiseBc7Dds(allocator, case.width, case.height, case.mip_count, case.array_size);
        defer allocator.free(bytes);

        var file = try parse(allocator, bytes);
        defer file.deinit(allocator);

        try std.testing.expectEqual(case.mip_count, file.mip_count);
        try std.testing.expectEqual(case.array_size, file.array_size);
        try std.testing.expectEqual(vk.Format.bc7_unorm_block, file.format);

        for (case.rows) |row| {
            const image = file.imageData(row.mip, row.layer);

            try std.testing.expectEqual(row.w, image.width);
            try std.testing.expectEqual(row.h, image.height);
            try std.testing.expectEqual(@as(u32, 1), image.depth);
            try std.testing.expectEqual(row.pitch, image.row_pitch);
            try std.testing.expectEqual(row.slice, image.slice_pitch);
            try std.testing.expectEqual(row.first, image.data[0]);
            try std.testing.expectEqual(row.last, image.data[image.data.len - 1]);
        }
    }
}

test "rejects malformed input instead of reading past the end" {
    const allocator = std.testing.allocator;

    try std.testing.expectError(error.TruncatedDds, parse(allocator, "DDS "));
    try std.testing.expectError(error.NotADdsFile, parse(allocator, &[_]u8{0} ** 256));

    // A valid header chain whose payload was cut off.
    var buffer = [_]u8{0} ** (4 + @sizeOf(Header) + @sizeOf(HeaderDxt10) + 16);
    std.mem.writeInt(u32, buffer[0..4], magic, .little);

    var header = std.mem.zeroes(Header);
    header.size = @sizeOf(Header);
    header.width = 2048;
    header.height = 2048;
    header.pixel_format.size = @sizeOf(PixelFormatHeader);
    header.pixel_format.flags = pixel_format_flag_four_cc;
    header.pixel_format.four_cc = four_cc_dx10;
    @memcpy(buffer[4..][0..@sizeOf(Header)], std.mem.asBytes(&header));

    const dxt10 = HeaderDxt10{
        .dxgi_format = 98, // BC7_UNorm
        .resource_dimension = @intFromEnum(ResourceDimension.texture_2d),
        .misc_flag = 0,
        .array_size = 1,
        .misc_flags2 = 0,
    };
    @memcpy(buffer[4 + @sizeOf(Header) ..][0..@sizeOf(HeaderDxt10)], std.mem.asBytes(&dxt10));

    try std.testing.expectError(error.TruncatedDds, parse(allocator, &buffer));

    // Same file, unsupported DXGI format.
    var unsupported = dxt10;
    unsupported.dxgi_format = 2; // R32G32B32A32_Float
    @memcpy(buffer[4 + @sizeOf(Header) ..][0..@sizeOf(HeaderDxt10)], std.mem.asBytes(&unsupported));

    try std.testing.expectError(error.UnsupportedDdsFormat, parse(allocator, &buffer));
}
