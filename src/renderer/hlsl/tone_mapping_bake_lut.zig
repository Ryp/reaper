// Mirror of src/renderer/shader/tone_mapping_bake_lut.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("types.zig");

pub const ToneMappingBakeLUT_Res: hlsl.Uint = 1024;
pub const ToneMappingBakeLUT_ThreadCount: hlsl.Uint = 256;

pub const ToneMappingBakeLUT_Consts = extern struct {
    min_nits: hlsl.Float = 0,
    max_nits: hlsl.Float = 0,
};

comptime {
    hlsl.assertLayout(ToneMappingBakeLUT_Consts, 8);
}
