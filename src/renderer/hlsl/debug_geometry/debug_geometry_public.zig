// Mirror of src/renderer/shader/debug_geometry/debug_geometry_public.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

pub const DebugGeometryType_Icosphere: hlsl.Uint = 0;
pub const DebugGeometryType_Box: hlsl.Uint = 1;
pub const DebugGeometryTypeCount: hlsl.Uint = 2;

pub const DebugGeometryUserCommand = extern struct {
    ms_to_ws_matrix: hlsl.Float3x4 = .{},
    geometry_type: hlsl.Uint = 0,
    color_rgba8_unorm: hlsl.Uint = 0,
    // FIXME Think of a better way to have some kind of polymorphism that isn't super ugly
    _pad0: hlsl.Uint = 0,
    _pad1: hlsl.Uint = 0,
    half_extent: hlsl.Float3 = .{},
    _pad2: hlsl.Uint = 0,
};

pub const DebugGeometryUserCommandSizeBytes: hlsl.Uint = 4 * (16 + 4 + 4);

comptime {
    hlsl.assertLayout(DebugGeometryUserCommand, DebugGeometryUserCommandSizeBytes);
}
