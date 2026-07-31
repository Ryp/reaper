// Mirror of src/renderer/shader/debug_gradient.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const DebugGradientThreadCountX: hlsl.Uint = 8;
pub const DebugGradientThreadCountY: hlsl.Uint = 8;

pub const DebugGradientPushConstants = extern struct {
    extent_ts: hlsl.Uint2 = .{},
    extent_ts_inv: hlsl.Float2 = .{},
};

comptime {
    hlsl.assertLayout(DebugGradientPushConstants, 16);
}
