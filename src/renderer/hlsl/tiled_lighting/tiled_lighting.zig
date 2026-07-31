// Mirror of src/renderer/shader/tiled_lighting/tiled_lighting.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

// Has to be a multiple of 8
pub const TileSizeX: hlsl.Uint = 16;
pub const TileSizeY: hlsl.Uint = 16;

// Has to be a multiple of TileSizeX
pub const TiledLightingThreadCountX: hlsl.Uint = 16;
pub const TiledLightingThreadCountY: hlsl.Uint = 16;

// Remove one to leave space for the count
pub const ElementsPerTile: hlsl.Uint = 16;
pub const LightsPerTileMax: hlsl.Uint = ElementsPerTile - 1;

pub const TileLightRasterPushConstants = extern struct {
    instance_id_offset: hlsl.Uint = 0,
    tile_count_x: hlsl.Uint = 0,
};

pub const LightVolumeInstance = extern struct {
    ms_to_cs: hlsl.Float4x4 = .{},
    cs_to_ms: hlsl.Float4x4 = .{},
    cs_to_vs: hlsl.Float4x4 = .{}, // FIXME
    vs_to_ms: hlsl.Float3x4 = .{},
    light_index: hlsl.Uint = 0,
    radius: hlsl.Float = 0,
    _pad0: hlsl.Uint = 0,
    _pad1: hlsl.Uint = 0,
};

pub const ProxyVolumeInstance = extern struct {
    ms_to_vs_with_scale: hlsl.Float3x4 = .{},
};

pub const TiledLightingConstants = extern struct {
    cs_to_vs: hlsl.Float4x4 = .{},
    vs_to_ws: hlsl.Float3x4 = .{},
};

// No need for padding here
pub const TiledLightingPushConstants = extern struct {
    extent_ts: hlsl.Uint2 = .{},
    extent_ts_inv: hlsl.Float2 = .{},
    tile_count_x: hlsl.Uint = 0,
};

pub const TileDebug = extern struct {
    light_count: hlsl.Uint = 0,
    _pad0: hlsl.Uint = 0,
    _pad1: hlsl.Uint = 0,
    _pad2: hlsl.Uint = 0,
};

// No need for padding here
pub const TiledLightingDebugPushConstants = extern struct {
    extent_ts: hlsl.Uint2 = .{},
    tile_count_x: hlsl.Uint = 0,
};

comptime {
    hlsl.assertLayout(TileLightRasterPushConstants, 8);
    hlsl.assertLayout(LightVolumeInstance, 272);
    hlsl.assertLayout(ProxyVolumeInstance, 64);
    hlsl.assertLayout(TiledLightingConstants, 128);
    hlsl.assertLayoutPadded(TiledLightingPushConstants, 20, 24);
    hlsl.assertLayout(TileDebug, 16);
    hlsl.assertLayoutPadded(TiledLightingDebugPushConstants, 12, 16);
}
