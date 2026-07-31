// Port of src/renderer/vulkan/MaterialResources.{h,cpp} and TextureLoadingPNG.cpp
//
// Textures are decoded on the CPU into one big staging buffer, then uploaded in
// a single batch at the top of the next frame: one barrier and one
// buffer-to-image copy per texture, then one barrier moving all of them to
// READ_ONLY_OPTIMAL together.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("barrier.zig");
const buffer_module = @import("buffer.zig");
const dds = @import("dds.zig");
const gpu_buffer = @import("../buffer/gpu_buffer.zig");
const gpu_texture_properties = @import("../texture/gpu_texture_properties.zig");
const gpu_texture_view = @import("../texture/gpu_texture_view.zig");
const image_module = @import("image.zig");
const lodepng = @import("lodepng.zig").c;
const mesh2 = @import("../mesh2.zig");
const vma = @import("vma.zig").c;

const log = std.log.scoped(.renderer);

const staging_buffer_size_bytes: u64 = 512 * 1024 * 1024;

pub const StagingEntry = struct {
    texture_properties: gpu_texture_properties.GPUTextureProperties,
    copy_command_offset: u32,
    copy_command_count: u32,
    target: vk.Image,
};

pub const TextureResource = struct {
    texture: image_module.GPUTexture,
    default_view: vk.ImageView,
};

pub const ResourceStagingArea = struct {
    offset_bytes: u64 = 0,
    buffer_properties: gpu_buffer.GPUBufferProperties,
    staging_buffer: buffer_module.GPUBuffer,

    /// One buffer copy region per mip level.
    buffer_copy_regions: std.ArrayList(vk.BufferImageCopy2) = .empty,
    staging_queue: std.ArrayList(StagingEntry) = .empty,
};

pub const MaterialResources = struct {
    staging: ResourceStagingArea,
    textures: std.ArrayList(TextureResource) = .empty,

    allocator: std.mem.Allocator,

    pub fn init(vma_instance: vma.VmaAllocator, allocator: std.mem.Allocator) !MaterialResources {
        const properties = gpu_buffer.defaultBufferProperties(
            staging_buffer_size_bytes,
            @sizeOf(u8),
            .{ .transfer_src = true },
        );

        const staging_buffer = try buffer_module.createBuffer(vma_instance, properties, .cpu_only);

        return .{
            .staging = .{ .buffer_properties = properties, .staging_buffer = staging_buffer },
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *MaterialResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        for (self.textures.items) |texture| {
            vkd.destroyImageView(device, texture.default_view, null);
            image_module.destroyImage(vma_instance, texture.texture);
        }
        self.textures.deinit(self.allocator);

        self.staging.buffer_copy_regions.deinit(self.allocator);
        self.staging.staging_queue.deinit(self.allocator);

        buffer_module.destroyBuffer(vma_instance, self.staging.staging_buffer);
    }

    pub fn allocMaterialTextures(self: *MaterialResources, count: u32) !mesh2.HandleSpan(mesh2.TextureHandle) {
        const old_size: u32 = @intCast(self.textures.items.len);

        try self.textures.appendNTimes(self.allocator, .{
            .texture = .{ .handle = .null_handle, .allocation = null },
            .default_view = .null_handle,
        }, count);

        return .{ .offset = old_size, .count = count };
    }

    pub fn loadPngTextures(
        self: *MaterialResources,
        vkd: anytype,
        device: vk.Device,
        vma_instance: vma.VmaAllocator,
        filenames: []const []const u8,
        handle_span: mesh2.HandleSpan(mesh2.TextureHandle),
        is_srgb: []const bool,
    ) !void {
        std.debug.assert(handle_span.count == filenames.len);
        std.debug.assert(is_srgb.len == filenames.len);

        for (filenames, is_srgb, 0..) |filename, srgb, i| {
            var staging_entry = try copyPngToStagingArea(self, vma_instance, filename, srgb);

            self.textures.items[handle_span.offset + i] = try createTextureResource(
                self,
                vkd,
                device,
                vma_instance,
                &staging_entry,
            );
        }
    }

    /// Unlike PNG there is no sRGB flag: the DDS carries its own format, so
    /// whether a map decodes through the sRGB EOTF is baked into the file.
    pub fn loadDdsTextures(
        self: *MaterialResources,
        vkd: anytype,
        device: vk.Device,
        vma_instance: vma.VmaAllocator,
        io: std.Io,
        filenames: []const []const u8,
        handle_span: mesh2.HandleSpan(mesh2.TextureHandle),
    ) !void {
        std.debug.assert(handle_span.count == filenames.len);

        for (filenames, 0..) |filename, i| {
            var staging_entry = try copyDdsToStagingArea(self, vma_instance, io, filename);

            self.textures.items[handle_span.offset + i] = try createTextureResource(
                self,
                vkd,
                device,
                vma_instance,
                &staging_entry,
            );
        }
    }
};

fn createTextureResource(
    resources: *MaterialResources,
    vkd: anytype,
    device: vk.Device,
    vma_instance: vma.VmaAllocator,
    staging_entry: *StagingEntry,
) !TextureResource {
    const image_info = try image_module.createImage(vma_instance, staging_entry.texture_properties);

    staging_entry.target = image_info.handle;
    try resources.staging.staging_queue.append(resources.allocator, staging_entry.*);

    const view = gpu_texture_view.defaultTextureView(staging_entry.texture_properties);

    return .{
        .texture = image_info,
        .default_view = try image_module.createImageView(vkd, device, image_info.handle, view),
    };
}

/// Decodes a PNG and copies it into the staging buffer, recording the copy
/// region for the batched upload.
///
/// NOTE: driver support for linear tiled RGB textures is more limited than the
/// 4-channel version, so RGBA is preferred — same reasoning as the C++.
fn copyPngToStagingArea(
    resources: *MaterialResources,
    vma_instance: vma.VmaAllocator,
    file_path: []const u8,
    is_srgb: bool,
) !StagingEntry {
    var png_image_ptr: [*c]u8 = null;
    var width: c_uint = 0;
    var height: c_uint = 0;

    const path_z = try resources.allocator.dupeZ(u8, file_path);
    defer resources.allocator.free(path_z);

    const err = lodepng.lodepng_decode32_file(&png_image_ptr, &width, &height, path_z.ptr);
    if (err != 0) {
        log.err("lodepng error {} decoding '{s}': {s}", .{ err, file_path, lodepng.lodepng_error_text(err) });
        return error.PngDecodeFailed;
    }
    defer std.c.free(png_image_ptr);

    const size_bytes: u64 = @as(u64, width) * height * 4;
    const pixel_format: vk.Format = if (is_srgb) .r8g8b8a8_srgb else .r8g8b8a8_unorm;

    var properties = gpu_texture_properties.defaultTextureProperties(
        width,
        height,
        pixel_format,
        .{ .transfer_dst = true, .sampled = true },
    );
    properties.misc_flags = .{ .linear_tiling = true };

    const staging = &resources.staging;

    try buffer_module.uploadBufferData(
        vma_instance,
        staging.staging_buffer,
        staging.buffer_properties,
        png_image_ptr[0..@intCast(size_bytes)],
        @intCast(staging.offset_bytes),
    );

    const subresource = gpu_texture_view.defaultTextureSubresourceOneColorMip(0, 0);
    const command_offset: u32 = @intCast(staging.buffer_copy_regions.items.len);

    try staging.buffer_copy_regions.append(resources.allocator, .{
        .s_type = .buffer_image_copy_2,
        .p_next = null,
        .buffer_offset = staging.offset_bytes,
        .buffer_row_length = 0,
        .buffer_image_height = 0,
        .image_subresource = image_module.getImageSubresourceLayers(subresource),
        .image_offset = .{ .x = 0, .y = 0, .z = 0 },
        .image_extent = .{ .width = width, .height = height, .depth = 1 },
    });

    staging.offset_bytes += size_bytes;

    // OOB
    std.debug.assert(staging.offset_bytes < staging.buffer_properties.element_count);

    log.debug("loaded texture '{s}': {}x{} {s}", .{
        file_path,
        width,
        height,
        if (is_srgb) "sRGB" else "linear",
    });

    return .{
        .texture_properties = properties,
        .copy_command_offset = command_offset,
        .copy_command_count = 1,
        .target = .null_handle, // FIXME filled in by createTextureResource
    };
}

/// Same as the PNG path, except every mip of every layer gets its own copy
/// region and the pixel format comes from the file rather than a caller flag.
fn copyDdsToStagingArea(
    resources: *MaterialResources,
    vma_instance: vma.VmaAllocator,
    io: std.Io,
    file_path: []const u8,
) !StagingEntry {
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, file_path, resources.allocator, .limited(1 << 30));
    defer resources.allocator.free(bytes);

    var file = try dds.parse(resources.allocator, bytes);
    defer file.deinit(resources.allocator);

    if (file.dimension != .texture_2d) return error.UnsupportedDdsDimension;
    if (file.depth != 1) return error.UnsupportedDdsDepth;
    if (file.array_size != 1) return error.UnsupportedDdsArraySize;

    var properties = gpu_texture_properties.defaultTextureProperties(
        file.width,
        file.height,
        file.format,
        .{ .transfer_dst = true, .sampled = true },
    );
    properties.depth = file.depth;
    properties.mip_count = file.mip_count;
    properties.layer_count = file.array_size;

    const staging = &resources.staging;
    const command_offset: u32 = @intCast(staging.buffer_copy_regions.items.len);

    for (0..file.array_size) |layer| {
        for (0..file.mip_count) |mip| {
            const image = file.imageData(@intCast(mip), @intCast(layer));

            try buffer_module.uploadBufferData(
                vma_instance,
                staging.staging_buffer,
                staging.buffer_properties,
                image.data,
                @intCast(staging.offset_bytes),
            );

            const subresource = gpu_texture_view.defaultTextureSubresourceOneColorMip(
                @intCast(mip),
                @intCast(layer),
            );

            try staging.buffer_copy_regions.append(resources.allocator, .{
                .s_type = .buffer_image_copy_2,
                .p_next = null,
                .buffer_offset = staging.offset_bytes,
                .buffer_row_length = 0,
                .buffer_image_height = 0,
                .image_subresource = image_module.getImageSubresourceLayers(subresource),
                .image_offset = .{ .x = 0, .y = 0, .z = 0 },
                .image_extent = .{ .width = image.width, .height = image.height, .depth = image.depth },
            });

            staging.offset_bytes += image.data.len;

            // OOB
            std.debug.assert(staging.offset_bytes < staging.buffer_properties.element_count);
        }
    }

    log.debug("loaded texture '{s}': {}x{} {s}, {} mip(s)", .{
        file_path,
        file.width,
        file.height,
        @tagName(file.format),
        file.mip_count,
    });

    return .{
        .texture_properties = properties,
        .copy_command_offset = command_offset,
        .copy_command_count = file.mip_count * file.array_size,
        .target = .null_handle, // FIXME filled in by createTextureResource
    };
}

// --------------------------------------------------------------------------
// Upload
// --------------------------------------------------------------------------

const access_host = barrier_module.GPUTextureAccess{
    .stage_mask = .{ .host_bit = true },
    .access_mask = .{},
    .image_layout = .undefined,
};

const access_transfer_dst = barrier_module.GPUTextureAccess{
    .stage_mask = .{ .all_transfer_bit = true },
    .access_mask = .{ .transfer_write_bit = true },
    .image_layout = .transfer_dst_optimal,
};

const access_shader_read = barrier_module.GPUTextureAccess{
    .stage_mask = .{ .fragment_shader_bit = true, .compute_shader_bit = true },
    .access_mask = .{ .shader_read_bit = true },
    .image_layout = .read_only_optimal,
};

fn toBarrierSubresource(
    subresource: gpu_texture_view.GPUTextureSubresource,
) barrier_module.GPUTextureSubresource {
    return .{
        .aspect = subresource.aspect.toVk(),
        .mip_offset = subresource.mip_offset,
        .mip_count = subresource.mip_count,
        .layer_offset = subresource.layer_offset,
        .layer_count = subresource.layer_count,
    };
}

/// Records the batched upload. Does nothing once the queue has been drained,
/// so it is safe to call every frame.
pub fn recordUploadCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    resources: *MaterialResources,
    scratch: std.mem.Allocator,
) !void {
    const staging = &resources.staging;
    if (staging.staging_queue.items.len == 0) return;

    for (staging.staging_queue.items) |entry| {
        const subresource = gpu_texture_view.defaultTextureSubresource(entry.texture_properties);

        const barriers = [_]vk.ImageMemoryBarrier2{barrier_module.getImageBarrierSameQueue(
            entry.target,
            toBarrierSubresource(subresource),
            access_host,
            access_transfer_dst,
        )};

        vkd.cmdPipelineBarrier2(cmd_buffer, &barrier_module.getImageBarrierDependencyInfo(&barriers));

        const copy_regions = staging.buffer_copy_regions.items[entry.copy_command_offset..][0..entry.copy_command_count];

        const copy = vk.CopyBufferToImageInfo2{
            .s_type = .copy_buffer_to_image_info_2,
            .p_next = null,
            .src_buffer = staging.staging_buffer.handle,
            .dst_image = entry.target,
            .dst_image_layout = access_transfer_dst.image_layout,
            .region_count = @intCast(copy_regions.len),
            .p_regions = copy_regions.ptr,
        };

        vkd.cmdCopyBufferToImage2(cmd_buffer, &copy);
    }

    // One barrier for all of them together.
    const prerender_barriers = try scratch.alloc(vk.ImageMemoryBarrier2, staging.staging_queue.items.len);

    for (staging.staging_queue.items, prerender_barriers) |entry, *out| {
        const subresource = gpu_texture_view.defaultTextureSubresource(entry.texture_properties);

        out.* = barrier_module.getImageBarrierSameQueue(
            entry.target,
            toBarrierSubresource(subresource),
            access_transfer_dst,
            access_shader_read,
        );
    }

    vkd.cmdPipelineBarrier2(cmd_buffer, &barrier_module.getImageBarrierDependencyInfo(prerender_barriers));

    // Flush the staging area state.
    staging.offset_bytes = 0;
    staging.buffer_copy_regions.clearRetainingCapacity();
    staging.staging_queue.clearRetainingCapacity();
}
