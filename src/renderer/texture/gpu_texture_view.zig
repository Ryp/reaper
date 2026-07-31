// Port of src/renderer/texture/GPUTextureView.{h,cpp}

const vk = @import("vulkan");

const GPUTextureProperties = @import("gpu_texture_properties.zig").GPUTextureProperties;

pub const ViewAspect = packed struct(u32) {
    color: bool = false,
    depth: bool = false,
    stencil: bool = false,
    _reserved: u29 = 0,

    pub const color_only: ViewAspect = .{ .color = true };
    pub const depth_only: ViewAspect = .{ .depth = true };
    pub const depth_stencil: ViewAspect = .{ .depth = true, .stencil = true };
    pub const stencil_only: ViewAspect = .{ .stencil = true };

    pub fn toVk(self: ViewAspect) vk.ImageAspectFlags {
        return .{
            .color_bit = self.color,
            .depth_bit = self.depth,
            .stencil_bit = self.stencil,
        };
    }
};

pub const GPUTextureSubresource = struct {
    aspect: ViewAspect = .color_only,
    mip_offset: u32 = 0,
    mip_count: u32 = 1,
    layer_offset: u32 = 0,
    layer_count: u32 = 1,
};

pub const GPUTextureViewType = enum {
    tex_1d,
    tex_2d,
    tex_3d,
    tex_cube,
    tex_1d_array,
    tex_2d_array,
    tex_cube_array,
};

pub const GPUTextureView = struct {
    type: GPUTextureViewType = .tex_2d,
    format: vk.Format = .undefined,
    subresource: GPUTextureSubresource = .{},
};

pub fn defaultTextureSubresourceOneColorMip(mip_index: u32, layer_index: u32) GPUTextureSubresource {
    return .{
        .aspect = .color_only,
        .mip_offset = mip_index,
        .mip_count = 1,
        .layer_offset = layer_index,
        .layer_count = 1,
    };
}

pub fn defaultViewAspect(format: vk.Format) ViewAspect {
    return switch (format) {
        .d16_unorm, .d32_sfloat => .depth_only,
        .x8_d24_unorm_pack32, .d16_unorm_s8_uint, .d24_unorm_s8_uint, .d32_sfloat_s8_uint => .depth_stencil,
        .s8_uint => .stencil_only,
        else => .color_only,
    };
}

pub fn defaultTextureSubresource(properties: GPUTextureProperties) GPUTextureSubresource {
    return .{
        .aspect = defaultViewAspect(properties.format),
        .mip_offset = 0,
        .mip_count = properties.mip_count,
        .layer_offset = 0,
        .layer_count = properties.layer_count,
    };
}

pub fn defaultTextureView(properties: GPUTextureProperties) GPUTextureView {
    return .{
        .type = switch (properties.type) {
            .tex_1d => .tex_1d,
            .tex_2d => .tex_2d,
            .tex_3d => .tex_3d,
        },
        .format = properties.format,
        .subresource = defaultTextureSubresource(properties),
    };
}
