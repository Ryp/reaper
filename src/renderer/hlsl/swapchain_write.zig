// Mirror of src/renderer/shader/swapchain_write.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const COLOR_SPACE_SRGB: hlsl.Uint = 0;
pub const COLOR_SPACE_REC709: hlsl.Uint = 1;
pub const COLOR_SPACE_DISPLAY_P3: hlsl.Uint = 2;
pub const COLOR_SPACE_REC2020: hlsl.Uint = 3;

pub const TRANSFER_FUNC_LINEAR: hlsl.Uint = 0;
pub const TRANSFER_FUNC_SRGB: hlsl.Uint = 1;
pub const TRANSFER_FUNC_REC709: hlsl.Uint = 2;
pub const TRANSFER_FUNC_PQ: hlsl.Uint = 3;
pub const TRANSFER_FUNC_WINDOWS_SCRGB: hlsl.Uint = 4;

pub const DYNAMIC_RANGE_SDR: hlsl.Uint = 0;
pub const DYNAMIC_RANGE_HDR: hlsl.Uint = 1;

pub const SwapchainWriteParams = extern struct {
    exposure_compensation: hlsl.Float = 0,
    tonemap_min_nits: hlsl.Float = 0,
    tonemap_max_nits: hlsl.Float = 0,
    sdr_ui_max_brightness_nits: hlsl.Float = 0,
    sdr_peak_brightness_nits: hlsl.Float = 0,
};

comptime {
    hlsl.assertLayout(SwapchainWriteParams, 20);
}
