// Mirror of src/renderer/shader/meshlet/meshlet.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

pub const MeshletMaxTriangleCount: hlsl.Uint = 64;

pub const Meshlet = extern struct {
    index_offset: hlsl.Uint = 0,
    index_count: hlsl.Uint = 0,
    vertex_offset: hlsl.Uint = 0,
    vertex_count: hlsl.Uint = 0,
    center_ms: hlsl.Float3 = .{},
    radius: hlsl.Float = 0,
    cone_axis_ms: hlsl.Float3 = .{},
    cone_cutoff: hlsl.Float = 0, // = cos(angle/2)
    cone_apex_ms: hlsl.Float3 = .{},
    _pad: hlsl.Float = 0,
};

pub const VisibleMeshlet = extern struct {
    mesh_instance_id: hlsl.Uint = 0,
    visible_triangle_offset: hlsl.Uint = 0,
    vertex_offset: hlsl.Uint = 0,
    _pad: hlsl.Float = 0,
};

comptime {
    hlsl.assertLayout(Meshlet, 64);
    hlsl.assertLayout(VisibleMeshlet, 16);
}
