// Port of src/renderer/texture/GPUTextureProperties.{h,cpp}
//
// The C++ PixelFormat enum is deliberately not ported: vk.Format is used
// directly everywhere instead.

const vk = @import("vulkan");

pub const GPUTextureType = enum {
    tex_1d,
    tex_2d,
    tex_3d,
};

pub const GPUTextureUsage = packed struct(u32) {
    transfer_src: bool = false,
    transfer_dst: bool = false,
    sampled: bool = false,
    storage: bool = false,
    color_attachment: bool = false,
    depth_stencil_attachment: bool = false,
    transient_attachment: bool = false,
    input_attachment: bool = false,
    _reserved: u24 = 0,

    pub const none: GPUTextureUsage = .{};
};

pub const GPUTextureMisc = packed struct(u32) {
    linear_tiling: bool = false,
    sample_location_compatible: bool = false,
    _reserved: u30 = 0,

    pub const none: GPUTextureMisc = .{};
};

pub const GPUTextureProperties = struct {
    type: GPUTextureType = .tex_2d,
    width: u32 = 0,
    height: u32 = 0,
    depth: u32 = 1,
    format: vk.Format = .undefined,
    mip_count: u32 = 1,
    layer_count: u32 = 1,
    sample_count: u32 = 1,
    usage_flags: GPUTextureUsage = .none,
    misc_flags: GPUTextureMisc = .none,
};

pub fn defaultTextureProperties(
    width: u32,
    height: u32,
    format: vk.Format,
    usage_flags: GPUTextureUsage,
) GPUTextureProperties {
    return .{
        .type = .tex_2d,
        .width = width,
        .height = height,
        .depth = 1,
        .format = format,
        .mip_count = 1,
        .layer_count = 1,
        .sample_count = 1,
        .usage_flags = usage_flags,
        .misc_flags = .none,
    };
}
