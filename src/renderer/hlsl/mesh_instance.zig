// Mirror of src/renderer/shader/mesh_instance.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

// FIXME move out?
pub const MaterialTextureMaxCount: hlsl.Uint = 16;
pub const ShadowMapMaxCount: hlsl.Uint = 8;

pub const MeshInstance = extern struct {
    ms_to_cs_matrix: hlsl.Float4x4 = .{}, // FIXME
    ms_to_ws_matrix: hlsl.Float3x4 = .{},
    normal_ms_to_vs_matrix: hlsl.Float3x3 = .{},
    _pad: hlsl.Uint3 = .{},
    material_index: hlsl.Uint = 0,
};

comptime {
    hlsl.assertLayout(MeshInstance, 192);
}
