// Mirror of src/renderer/shader/lighting.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const InvalidShadowMapIndex: hlsl.Uint = 0xFFFFFFFF;

pub const PointLightProperties = extern struct {
    light_ws_to_cs: hlsl.Float4x4 = .{}, // FIXME xy could be in uv-space already
    position_vs: hlsl.Float3 = .{},
    intensity: hlsl.Float = 0,
    color: hlsl.Float3 = .{},
    radius_sq: hlsl.Float = 0,
    shadow_map_index: hlsl.Uint = 0,
    _pad0: hlsl.Uint = 0,
    _pad1: hlsl.Uint = 0,
    _pad2: hlsl.Uint = 0,
};

comptime {
    hlsl.assertLayout(PointLightProperties, 112);
}
