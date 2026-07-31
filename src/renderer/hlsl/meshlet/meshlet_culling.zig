// Mirror of src/renderer/shader/meshlet/meshlet_culling.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

pub const MeshletOffsets = extern struct {
    index_offset: hlsl.Uint = 0, // Has the global index offset baked in
    index_count: hlsl.Uint = 0,
    vertex_offset: hlsl.Uint = 0, // Has the global vertex offset baked in
    cull_instance_id: hlsl.Uint = 0,
};

pub const PrepareIndirectDispatchThreadCount: hlsl.Uint = 64;
pub const MeshletCullThreadCount: hlsl.Uint = 64;

pub const CountersCount: hlsl.Uint = 4; // Align on 16 byte boundary

// Counter buffer layout
pub const MeshletCounterOffset: hlsl.Uint = 0;
pub const TriangleCounterOffset: hlsl.Uint = 1;
pub const DrawCommandCounterOffset: hlsl.Uint = 2;

pub const CullMeshletPushConstants = extern struct {
    meshlet_offset: hlsl.Uint = 0,
    meshlet_count: hlsl.Uint = 0,
    first_index: hlsl.Uint = 0,
    first_vertex: hlsl.Uint = 0,
    cull_instance_offset: hlsl.Uint = 0,
    // No need for manual padding for push constants
};

pub const CullPushConstants = extern struct {
    output_size_ts: hlsl.Float2 = .{},
    main_pass: hlsl.Uint = 0,
    // No need for manual padding for push constants
};

pub const CullMeshInstanceParams = extern struct {
    ms_to_cs_matrix: hlsl.Float4x4 = .{},
    vs_to_ms_matrix_translate: hlsl.Float3 = .{}, // FIXME Put this in somewhere else
    instance_id: hlsl.Uint = 0,
};

comptime {
    hlsl.assertLayout(MeshletOffsets, 16);
    hlsl.assertLayout(CullMeshletPushConstants, 20);
    hlsl.assertLayoutPadded(CullPushConstants, 12, 16);
    hlsl.assertLayout(CullMeshInstanceParams, 80);
}
