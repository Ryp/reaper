// Mirror of src/renderer/shader/histogram/reduce_histogram.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

pub const HistogramEVCount: hlsl.Float = 32.0;
pub const HistogramEVOffset: hlsl.Float = 16.0;
pub const HistogramRes: hlsl.Uint = 128;

pub const HistogramThreadCountX: hlsl.Uint = 8;
pub const HistogramThreadCountY: hlsl.Uint = 8;

pub const ReduceHDRPassParams = extern struct {
    extent_ts: hlsl.Uint2 = .{},
    extent_ts_inv: hlsl.Float2 = .{},
};

comptime {
    hlsl.assertLayout(ReduceHDRPassParams, 16);
}
