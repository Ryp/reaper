// Mirror of src/renderer/shader/forward.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const ForwardPassParams = extern struct {
    ws_to_vs_matrix: hlsl.Float3x4 = .{},
    ws_to_cs_matrix: hlsl.Float4x4 = .{},
    _pad: hlsl.Uint3 = .{},
    point_light_count: hlsl.Uint = 0,
};

comptime {
    hlsl.assertLayout(ForwardPassParams, 144);
}
