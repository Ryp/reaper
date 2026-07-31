// Mirror of src/renderer/shader/debug_geometry/debug_geometry_private.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

const public = @import("debug_geometry_public.zig");

pub const DebugGeometryMeshAlloc = extern struct {
    index_offset: hlsl.Uint = 0,
    index_count: hlsl.Uint = 0,
    vertex_offset: hlsl.Uint = 0,
    _pad: hlsl.Uint = 0,
};

pub const DebugGeometryBuildCmdsPassConstants = extern struct {
    main_camera_ws_to_cs: hlsl.Float4x4 = .{},
    debug_geometry_allocs: [public.DebugGeometryTypeCount]DebugGeometryMeshAlloc = @splat(.{}),
};

pub const DebugGeometryBuildCmdsThreadCount: hlsl.Uint = 128;

pub const DebugGeometryInstance = extern struct {
    ms_to_cs: hlsl.Float4x4 = .{},
    color: hlsl.Float3 = .{},
    _pad0: hlsl.Float = 0,
    half_extent: hlsl.Float3 = .{},
    _pad1: hlsl.Float = 0,
};

pub const DebugGeometryInstanceSizeBytes: hlsl.Uint = 4 * (16 + 4 + 4);

comptime {
    hlsl.assertLayout(DebugGeometryMeshAlloc, 16);
    hlsl.assertLayout(DebugGeometryBuildCmdsPassConstants, 64 + 16 * public.DebugGeometryTypeCount);
    hlsl.assertLayout(DebugGeometryInstance, DebugGeometryInstanceSizeBytes);
}
