// Mirror of src/renderer/shader/gbuffer/gbuffer_write_opaque.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

// Descriptor slot numbers only — this share file declares no structs.

pub const Slot_instance_params = 0;
pub const Slot_visible_meshlets = 1;
pub const Slot_buffer_position_ms = 2;
pub const Slot_buffer_attributes = 3;
pub const Slot_mesh_materials = 4;
pub const Slot_diffuse_map_sampler = 5;
pub const Slot_material_maps = 6;
