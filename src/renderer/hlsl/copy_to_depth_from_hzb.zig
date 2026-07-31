// Mirror of src/renderer/shader/copy_to_depth_from_hzb.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const CopyDepthFromHZBPushConstants = extern struct {
    copy_min: hlsl.Uint = 0,
};

comptime {
    hlsl.assertLayout(CopyDepthFromHZBPushConstants, 4);
}
