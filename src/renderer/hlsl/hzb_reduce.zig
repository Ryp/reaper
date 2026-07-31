// Mirror of src/renderer/shader/hzb_reduce.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const MinWaveLaneCount: hlsl.Uint = 8; // FIXME
pub const HZBMaxMipCount: hlsl.Uint = 10;

pub const HZBReduceThreadCountX: hlsl.Uint = 8;
pub const HZBReduceThreadCountY: hlsl.Uint = 8;

pub const HZBReducePushConstants = extern struct {
    depth_extent_ts_inv: hlsl.Float2 = .{},
};

comptime {
    hlsl.assertLayout(HZBReducePushConstants, 8);
}
