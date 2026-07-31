// Mirror of src/renderer/shader/mesh_material.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const MeshMaterial = extern struct {
    albedo_texture_index: hlsl.Uint = 0,
    roughness_texture_index: hlsl.Uint = 0,
    normal_texture_index: hlsl.Uint = 0,
    ao_texture_index: hlsl.Uint = 0,
};

comptime {
    hlsl.assertLayout(MeshMaterial, 16);
}
