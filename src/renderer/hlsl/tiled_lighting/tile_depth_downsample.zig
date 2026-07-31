// Mirror of src/renderer/shader/tiled_lighting/tile_depth_downsample.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

// NOTE: Should always be a power of two
pub const MinWaveLaneCount: hlsl.Uint = 8;

pub const TileDepthThreadCountX: hlsl.Uint = 8;
pub const TileDepthThreadCountY: hlsl.Uint = 8;

pub const TileDepthConstants = extern struct {
    extent_ts: hlsl.Uint2 = .{},
    extent_ts_inv: hlsl.Float2 = .{},
};

comptime {
    hlsl.assertLayout(TileDepthConstants, 16);
}
