// Mirror of src/renderer/shader/shadow/shadow_map_pass.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

pub const ShadowMapInstanceParams = extern struct {
    ms_to_cs_matrix: hlsl.Float4x4 = .{},
};

comptime {
    hlsl.assertLayout(ShadowMapInstanceParams, 64);
}
