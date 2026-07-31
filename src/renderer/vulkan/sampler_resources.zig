// Port of src/renderer/vulkan/SamplerResources.{h,cpp}
//
// NOTE: linear_clamp and linear_black_border are byte-for-byte identical in the
// C++ despite their names — linear_black_border uses CLAMP_TO_EDGE, not
// CLAMP_TO_BORDER. Both are kept so the two names stay available to the passes
// that reference them; fixing the address mode would change what those passes
// sample outside [0,1].

const std = @import("std");
const vk = @import("vulkan");

const shadow_use_reverse_z = @import("../prepare_buckets.zig").shadow_use_reverse_z;

pub const SamplerResources = struct {
    linear_clamp: vk.Sampler,
    linear_black_border: vk.Sampler,
    shadow_map_sampler: vk.Sampler,
    diffuse_map_sampler: vk.Sampler,

    pub fn init(vkd: anytype, device: vk.Device) !SamplerResources {
        const linear_clamp_info = vk.SamplerCreateInfo{
            .s_type = .sampler_create_info,
            .p_next = null,
            .flags = .{},
            .mag_filter = .linear,
            .min_filter = .linear,
            .mipmap_mode = .nearest,
            .address_mode_u = .clamp_to_edge,
            .address_mode_v = .clamp_to_edge,
            .address_mode_w = .clamp_to_edge,
            .mip_lod_bias = 0.0,
            .anisotropy_enable = .false,
            .max_anisotropy = 0,
            .compare_enable = .false,
            .compare_op = .always,
            .min_lod = 0.0,
            .max_lod = std.math.floatMax(f32),
            .border_color = .int_opaque_black,
            .unnormalized_coordinates = .false,
        };

        const linear_clamp = try vkd.createSampler(device, &linear_clamp_info, null);
        errdefer vkd.destroySampler(device, linear_clamp, null);

        // Same create info as above — see the note at the top of the file.
        const linear_black_border = try vkd.createSampler(device, &linear_clamp_info, null);
        errdefer vkd.destroySampler(device, linear_black_border, null);

        const shadow_map_info = vk.SamplerCreateInfo{
            .s_type = .sampler_create_info,
            .p_next = null,
            .flags = .{},
            .mag_filter = .linear,
            .min_filter = .linear,
            .mipmap_mode = .nearest,
            .address_mode_u = .clamp_to_border,
            .address_mode_v = .clamp_to_border,
            .address_mode_w = .clamp_to_border,
            .mip_lod_bias = 0.0,
            .anisotropy_enable = .false,
            .max_anisotropy = 0,
            .compare_enable = .true,
            .compare_op = if (shadow_use_reverse_z) .greater else .less,
            .min_lod = 0.0,
            .max_lod = std.math.floatMax(f32),
            .border_color = if (shadow_use_reverse_z) .int_opaque_black else .int_opaque_white,
            .unnormalized_coordinates = .false,
        };

        const shadow_map_sampler = try vkd.createSampler(device, &shadow_map_info, null);
        errdefer vkd.destroySampler(device, shadow_map_sampler, null);

        const diffuse_map_info = vk.SamplerCreateInfo{
            .s_type = .sampler_create_info,
            .p_next = null,
            .flags = .{},
            .mag_filter = .linear,
            .min_filter = .linear,
            .mipmap_mode = .linear,
            .address_mode_u = .repeat,
            .address_mode_v = .repeat,
            .address_mode_w = .repeat,
            .mip_lod_bias = 0.0,
            .anisotropy_enable = .true,
            .max_anisotropy = 8,
            .compare_enable = .false,
            .compare_op = .always,
            .min_lod = 0.0,
            .max_lod = std.math.floatMax(f32),
            .border_color = .int_opaque_black,
            .unnormalized_coordinates = .false,
        };

        const diffuse_map_sampler = try vkd.createSampler(device, &diffuse_map_info, null);

        return .{
            .linear_clamp = linear_clamp,
            .linear_black_border = linear_black_border,
            .shadow_map_sampler = shadow_map_sampler,
            .diffuse_map_sampler = diffuse_map_sampler,
        };
    }

    pub fn deinit(self: *SamplerResources, vkd: anytype, device: vk.Device) void {
        vkd.destroySampler(device, self.diffuse_map_sampler, null);
        vkd.destroySampler(device, self.shadow_map_sampler, null);
        vkd.destroySampler(device, self.linear_black_border, null);
        vkd.destroySampler(device, self.linear_clamp, null);
    }
};
