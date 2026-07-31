// Mirror of src/renderer/shader/vis_buffer/fill_gbuffer.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

pub const Slot_VisBuffer = 0;
pub const Slot_VisBufferDepthMS = 1;
pub const Slot_ResolvedDepth = 2;
pub const Slot_GBuffer0 = 3;
pub const Slot_GBuffer1 = 4;
pub const Slot_instance_params = 5;
pub const Slot_visible_index_buffer = 6;
pub const Slot_buffer_position_ms = 7;
pub const Slot_buffer_attributes = 8;
pub const Slot_visible_meshlets = 9;
pub const Slot_mesh_materials = 10;
pub const Slot_diffuse_map_sampler = 11;
pub const Slot_material_maps = 12;

pub const GBufferFillThreadCountX: hlsl.Uint = 16;
pub const GBufferFillThreadCountY: hlsl.Uint = 16;

pub const FillGBufferPushConstants = extern struct {
    extent_ts: hlsl.Uint2 = .{},
    extent_ts_inv: hlsl.Float2 = .{},
};

comptime {
    hlsl.assertLayout(FillGBufferPushConstants, 16);
}
