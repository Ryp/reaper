// Second test root (`zig build test`).
//
// Only GPU-free modules belong here: the suite has to stay runnable in CI on a
// machine with no Vulkan device.

comptime {
    _ = @import("math/linalg.zig");
    _ = @import("mesh/mesh.zig");
    _ = @import("renderer/camera.zig");
    _ = @import("renderer/mesh2.zig");
    _ = @import("renderer/prepare_buckets.zig");
    _ = @import("mesh/meshlet_builder.zig");
    _ = @import("mesh/obj_loader.zig");
    _ = @import("renderer/graph/graph_test.zig");
    _ = @import("renderer/vulkan/compute_helper.zig");
    _ = @import("renderer/vulkan/renderpass/forward.zig");
    _ = @import("renderer/vulkan/renderpass/meshlet_culling.zig");
    _ = @import("renderer/vulkan/shader_modules.zig");

    // Importing every CPU/GPU shared-struct mirror runs its comptime layout
    // asserts, which is the whole point of them.
    _ = @import("renderer/hlsl/types.zig");
    _ = @import("renderer/hlsl/copy_to_depth_from_hzb.zig");
    _ = @import("renderer/hlsl/debug_geometry/debug_geometry_private.zig");
    _ = @import("renderer/hlsl/debug_geometry/debug_geometry_public.zig");
    _ = @import("renderer/hlsl/forward.zig");
    _ = @import("renderer/hlsl/gbuffer/gbuffer_write_opaque.zig");
    _ = @import("renderer/hlsl/histogram/reduce_histogram.zig");
    _ = @import("renderer/hlsl/hzb_reduce.zig");
    _ = @import("renderer/hlsl/lighting.zig");
    _ = @import("renderer/hlsl/mesh_instance.zig");
    _ = @import("renderer/hlsl/mesh_material.zig");
    _ = @import("renderer/hlsl/meshlet/meshlet.zig");
    _ = @import("renderer/hlsl/meshlet/meshlet_culling.zig");
    _ = @import("renderer/hlsl/reduce_exposure.zig");
    _ = @import("renderer/hlsl/shadow/shadow_map_pass.zig");
    _ = @import("renderer/hlsl/sound/sound.zig");
    _ = @import("renderer/hlsl/swapchain_write.zig");
    _ = @import("renderer/hlsl/tiled_lighting/classify_volume.zig");
    _ = @import("renderer/hlsl/tiled_lighting/tile_depth_downsample.zig");
    _ = @import("renderer/hlsl/tiled_lighting/tiled_lighting.zig");
    _ = @import("renderer/hlsl/tone_mapping_bake_lut.zig");
    _ = @import("renderer/hlsl/vis_buffer/fill_gbuffer.zig");
}
