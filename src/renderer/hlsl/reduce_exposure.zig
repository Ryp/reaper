// Mirror of src/renderer/shader/reduce_exposure.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const MinWaveLaneCount: hlsl.Uint = 8; // FIXME

pub const ExposureThreadCountX: hlsl.Uint = 8;
pub const ExposureThreadCountY: hlsl.Uint = 8;

pub const ReduceExposurePassParams = extern struct {
    extent_ts: hlsl.Uint2 = .{},
    extent_ts_inv: hlsl.Float2 = .{},
};

pub const ReduceExposureTailPassParams = extern struct {
    extent_ts: hlsl.Uint2 = .{},
    extent_ts_inv: hlsl.Float2 = .{},
    tail_extent_ts: hlsl.Uint2 = .{},
    last_thread_group_index: hlsl.Uint = 0,
};

comptime {
    hlsl.assertLayout(ReduceExposurePassParams, 16);
    hlsl.assertLayoutPadded(ReduceExposureTailPassParams, 28, 32);
}
