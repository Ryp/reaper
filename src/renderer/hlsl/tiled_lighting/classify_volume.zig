// Mirror of src/renderer/shader/tiled_lighting/classify_volume.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

pub const ClassifyVolumeThreadCount: hlsl.Uint = 64;

pub const ClassifyVolumePushConstants = extern struct {
    vertex_offset: hlsl.Uint = 0,
    vertex_count: hlsl.Uint = 0,
    instance_id_offset: hlsl.Uint = 0,
    near_clip_plane_depth_vs: hlsl.Float = 0,
};

pub const InnerCounterOffsetBytes: hlsl.Uint = 0;
pub const OuterCounterOffsetBytes: hlsl.Uint = 4;

comptime {
    hlsl.assertLayout(ClassifyVolumePushConstants, 16);
}
