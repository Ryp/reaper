// Port of src/renderer/vulkan/renderpass/Constants.h and ForwardPassConstants.h
//
// PixelFormat is not ported, so these are vk.Format directly.

const vk = @import("vulkan");

pub const main_pass_use_reverse_z = true;
pub const main_pass_depth_format: vk.Format = .d16_unorm;

pub const forward_hdr_color_format: vk.Format = .b10g11r11_ufloat_pack32;
