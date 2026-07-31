// Port of src/renderer/vulkan/Image.{h,cpp}
//
// Roughly 620 of the C++ file's 800 lines are the PixelFormat<->VkFormat
// conversion tables. Those are gone: vk.Format is used directly throughout, so
// only the creation code and the flag translation remain.

const std = @import("std");
const vk = @import("vulkan");

const vma = @import("vma.zig").c;
const gpu_texture_properties = @import("../texture/gpu_texture_properties.zig");
const gpu_texture_view = @import("../texture/gpu_texture_view.zig");

const GPUTextureProperties = gpu_texture_properties.GPUTextureProperties;
const GPUTextureSubresource = gpu_texture_view.GPUTextureSubresource;
const GPUTextureView = gpu_texture_view.GPUTextureView;

pub const GPUTexture = struct {
    handle: vk.Image,
    allocation: vma.VmaAllocation,
};

pub fn sampleCountToVulkan(sample_count: u32) vk.SampleCountFlags {
    std.debug.assert(sample_count > 0);
    std.debug.assert(sample_count <= 64);
    std.debug.assert(std.math.isPowerOfTwo(sample_count));

    return @bitCast(sample_count);
}

pub fn getVulkanCreateFlags(properties: GPUTextureProperties) vk.ImageCreateFlags {
    return .{
        .sample_locations_compatible_depth_bit_ext = properties.misc_flags.sample_location_compatible,
    };
}

pub fn getVulkanUsageFlags(usage_flags: gpu_texture_properties.GPUTextureUsage) vk.ImageUsageFlags {
    return .{
        .transfer_src_bit = usage_flags.transfer_src,
        .transfer_dst_bit = usage_flags.transfer_dst,
        .sampled_bit = usage_flags.sampled,
        .storage_bit = usage_flags.storage,
        .color_attachment_bit = usage_flags.color_attachment,
        .depth_stencil_attachment_bit = usage_flags.depth_stencil_attachment,
        .transient_attachment_bit = usage_flags.transient_attachment,
        .input_attachment_bit = usage_flags.input_attachment,
    };
}

pub fn getImageSubresourceRange(subresource: GPUTextureSubresource) vk.ImageSubresourceRange {
    return .{
        .aspect_mask = subresource.aspect.toVk(),
        .base_mip_level = subresource.mip_offset,
        .level_count = subresource.mip_count,
        .base_array_layer = subresource.layer_offset,
        .layer_count = subresource.layer_count,
    };
}

pub fn getImageSubresourceLayers(subresource: GPUTextureSubresource) vk.ImageSubresourceLayers {
    std.debug.assert(subresource.mip_count == 1);

    return .{
        .aspect_mask = subresource.aspect.toVk(),
        .mip_level = subresource.mip_offset,
        .base_array_layer = subresource.layer_offset,
        .layer_count = subresource.layer_count,
    };
}

fn textureTypeToImageType(texture_type: gpu_texture_properties.GPUTextureType) vk.ImageType {
    return switch (texture_type) {
        .tex_1d => .@"1d",
        .tex_2d => .@"2d",
        .tex_3d => .@"3d",
    };
}

fn textureViewTypeToImageViewType(view_type: gpu_texture_view.GPUTextureViewType) vk.ImageViewType {
    return switch (view_type) {
        .tex_1d => .@"1d",
        .tex_2d => .@"2d",
        .tex_3d => .@"3d",
        .tex_cube => .cube,
        .tex_1d_array => .@"1d_array",
        .tex_2d_array => .@"2d_array",
        .tex_cube_array => .cube_array,
    };
}

pub fn createImage(
    allocator: vma.VmaAllocator,
    properties: GPUTextureProperties,
) !GPUTexture {
    std.debug.assert(!((properties.height > 1 or properties.depth > 1) and properties.type == .tex_1d));
    std.debug.assert(!(properties.depth > 1 and properties.type == .tex_2d));

    const tiling_mode: vk.ImageTiling = if (properties.misc_flags.linear_tiling) .linear else .optimal;

    const image_info = vk.ImageCreateInfo{
        .s_type = .image_create_info,
        .p_next = null,
        .flags = getVulkanCreateFlags(properties),
        .image_type = textureTypeToImageType(properties.type),
        .format = properties.format,
        .extent = .{ .width = properties.width, .height = properties.height, .depth = properties.depth },
        .mip_levels = properties.mip_count,
        .array_layers = properties.layer_count,
        .samples = sampleCountToVulkan(properties.sample_count),
        .tiling = tiling_mode,
        .usage = getVulkanUsageFlags(properties.usage_flags),
        .sharing_mode = .exclusive,
        .queue_family_index_count = 0,
        .p_queue_family_indices = null,
        .initial_layout = .undefined,
    };

    var alloc_info = std.mem.zeroes(vma.VmaAllocationCreateInfo);
    alloc_info.usage = vma.VMA_MEMORY_USAGE_GPU_ONLY;

    var image: vma.VkImage = null;
    var allocation: vma.VmaAllocation = null;

    const result = vma.vmaCreateImage(
        allocator,
        @ptrCast(&image_info),
        &alloc_info,
        &image,
        &allocation,
        null,
    );
    if (result != 0) return error.ImageCreationFailed;

    return .{
        .handle = @enumFromInt(@intFromPtr(image)),
        .allocation = allocation,
    };
}

pub fn destroyImage(allocator: vma.VmaAllocator, texture: GPUTexture) void {
    vma.vmaDestroyImage(allocator, @ptrFromInt(@intFromEnum(texture.handle)), texture.allocation);
}

pub fn createImageView(vkd: anytype, device: vk.Device, image: vk.Image, view: GPUTextureView) !vk.ImageView {
    std.debug.assert(view.subresource.mip_count > 0);
    std.debug.assert(view.subresource.layer_count > 0);

    const create_info = vk.ImageViewCreateInfo{
        .s_type = .image_view_create_info,
        .p_next = null,
        .flags = .{},
        .image = image,
        .view_type = textureViewTypeToImageViewType(view.type),
        .format = view.format,
        .components = .{ .r = .identity, .g = .identity, .b = .identity, .a = .identity },
        .subresource_range = getImageSubresourceRange(view.subresource),
    };

    return vkd.createImageView(device, &create_info, null);
}
