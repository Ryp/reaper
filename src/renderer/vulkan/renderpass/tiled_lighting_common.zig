// Port of src/renderer/vulkan/renderpass/TiledLightingCommon.{h,cpp}
//
// Per-frame CPU prep for the tiled lighting path: how many tiles the viewport
// splits into, and one light-volume + proxy-volume instance per scene light.
// The proxy volume is what the raster pass draws to find which tiles a light
// touches, so it is inflated to cover whole tiles.

const std = @import("std");

const camera_module = @import("../../camera.zig");
const compute_helper = @import("../compute_helper.zig");
const linalg = @import("../../../math/linalg.zig");
const prepare_buckets = @import("../../prepare_buckets.zig");

const hlsl = @import("../../hlsl/types.zig");
const hlsl_tiled_lighting = @import("../../hlsl/tiled_lighting/tiled_lighting.zig");

const Vec2 = linalg.Vec2;
const Vec3 = linalg.Vec3;

/// Inflating the proxy volume by a tile's worth of angle makes the raster pass
/// conservative: a light that only clips a tile's corner still lights it.
const tile_lighting_enable_conservative_raster = true;

/// Applied on top of the computed scale. FIXME magic number, verbatim from the
/// C++.
const conservative_scale_fudge: f32 = 1.6;

pub const TiledLightingFrame = struct {
    tile_count_x: u32 = 0,
    tile_count_y: u32 = 0,

    light_volumes: std.ArrayList(hlsl_tiled_lighting.LightVolumeInstance) = .empty,
    proxy_volumes: std.ArrayList(hlsl_tiled_lighting.ProxyVolumeInstance) = .empty,

    pub fn deinit(self: *TiledLightingFrame, allocator: std.mem.Allocator) void {
        self.light_volumes.deinit(allocator);
        self.proxy_volumes.deinit(allocator);
    }

    pub fn clear(self: *TiledLightingFrame) void {
        self.light_volumes.clearRetainingCapacity();
        self.proxy_volumes.clearRetainingCapacity();
    }
};

/// Math here is not exact, it's assuming a lot to keep it simple. There's not a
/// big perf loss normally.
fn computeAabbConservativeScale(
    aabb_half_extent: Vec3,
    distance_to_camera: f32,
    fov_radians: Vec2,
    tile_count_inv: Vec2,
) f32 {
    if (!tile_lighting_enable_conservative_raster) return 1.0;

    const epsilon: f32 = 0.0001;

    std.debug.assert(fov_radians[0] > 0.0);
    std.debug.assert(fov_radians[1] > 0.0);

    const aabb_radius_underestimate = @min(@min(aabb_half_extent[0], aabb_half_extent[1]), aabb_half_extent[2]);

    const aabb_projected_angular_diameter =
        2.0 * std.math.atan(aabb_radius_underestimate / @max(epsilon, distance_to_camera));

    const aabb_fov_ratio_x = aabb_projected_angular_diameter / fov_radians[0];
    const aabb_fov_ratio_y = aabb_projected_angular_diameter / fov_radians[1];

    const padded_fov_ratio_x = aabb_fov_ratio_x + tile_count_inv[0];
    const padded_fov_ratio_y = aabb_fov_ratio_y + tile_count_inv[1];

    const scale_factor_x = padded_fov_ratio_x / @max(aabb_fov_ratio_x, epsilon);
    const scale_factor_y = padded_fov_ratio_y / @max(aabb_fov_ratio_y, epsilon);

    const conservative_scale = @max(scale_factor_x, scale_factor_y);
    std.debug.assert(conservative_scale >= 1.0);

    return conservative_scale;
}

pub fn prepareTileLightingFrame(
    allocator: std.mem.Allocator,
    scene: *const prepare_buckets.SceneGraph,
    main_camera: camera_module.RendererPerspectiveCamera,
    frame: *TiledLightingFrame,
) !void {
    frame.tile_count_x = compute_helper.divRoundUp(
        main_camera.viewport.extent[0],
        hlsl_tiled_lighting.TileSizeX,
    );
    frame.tile_count_y = compute_helper.divRoundUp(
        main_camera.viewport.extent[1],
        hlsl_tiled_lighting.TileSizeY,
    );

    for (scene.scene_lights.items, 0..) |light, scene_light_index| {
        const light_ms_to_ws = prepare_buckets.getSceneNodeTransformSlow(light.scene_node);
        const light_ws_to_ms = linalg.mat4x3FromMat4(
            linalg.inverseMat4(linalg.mat4FromMat4x3(light_ms_to_ws)),
        );

        const light_position_ws = linalg.mulMat4x3Vec4(light_ms_to_ws, .{ 0, 0, 0, 1 });
        const light_position_vs = linalg.mulMat4x3Vec4(
            main_camera.ws_to_vs_matrix,
            linalg.vec4FromVec3(light_position_ws, 1.0),
        );

        const light_aabb_half_extent = Vec3{ light.radius, light.radius, light.radius };
        const light_distance = linalg.length(light_position_vs);

        const fov_radians = Vec2{
            main_camera.perspective_projection.half_fov_horizontal_radian,
            main_camera.perspective_projection.half_fov_vertical_radian,
        };
        const tile_count_inv = Vec2{
            1.0 / @as(f32, @floatFromInt(frame.tile_count_x)),
            1.0 / @as(f32, @floatFromInt(frame.tile_count_y)),
        };

        const conservative_scale = conservative_scale_fudge * computeAabbConservativeScale(
            light_aabb_half_extent,
            light_distance,
            fov_radians,
            tile_count_inv,
        );

        // FIXME what happens to scale inheriting from a transform hierarchy?
        // FIXME Fill completely
        // FIXME should handle scales from light shape
        // FIXME cs_to_vs not per-instance
        try frame.light_volumes.append(allocator, .{
            .ms_to_cs = hlsl.float4x4(linalg.mulMat4(
                main_camera.ws_to_cs_matrix,
                linalg.mat4FromMat4x3(light_ms_to_ws),
            )),
            .cs_to_ms = hlsl.float4x4(linalg.mulMat4(
                linalg.mat4FromMat4x3(light_ws_to_ms),
                main_camera.cs_to_ws_matrix,
            )),
            .cs_to_vs = hlsl.float4x4(main_camera.perspective_projection.cs_to_vs_matrix),
            .vs_to_ms = hlsl.float4x3(linalg.mat4x3FromMat4(linalg.mulMat4(
                linalg.mat4FromMat4x3(light_ws_to_ms),
                linalg.mat4FromMat4x3(main_camera.vs_to_ws_matrix),
            ))), // FIXME
            .light_index = @intCast(scene_light_index),
            // FIXME might be better baked in ms_to_cs
            .radius = light.radius * conservative_scale,
        });

        const volume_scale = linalg.scale(light_aabb_half_extent * linalg.splat(Vec3, conservative_scale));

        try frame.proxy_volumes.append(allocator, .{
            // FIXME mul order
            .ms_to_vs_with_scale = hlsl.float4x3(linalg.mat4x3FromMat4(linalg.mulMat4(
                linalg.mulMat4(linalg.mat4FromMat4x3(main_camera.ws_to_vs_matrix), linalg.mat4FromMat4x3(light_ms_to_ws)),
                volume_scale,
            ))),
        });
    }
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "tile counts round up so partial tiles are still covered" {
    // A viewport that is not a whole number of tiles still needs the trailing
    // partial tile, or its pixels get no lights at all.
    try testing.expectEqual(@as(u32, 1), compute_helper.divRoundUp(1, hlsl_tiled_lighting.TileSizeX));
    try testing.expectEqual(@as(u32, 1), compute_helper.divRoundUp(16, hlsl_tiled_lighting.TileSizeX));
    try testing.expectEqual(@as(u32, 2), compute_helper.divRoundUp(17, hlsl_tiled_lighting.TileSizeX));
    try testing.expectEqual(@as(u32, 80), compute_helper.divRoundUp(1280, hlsl_tiled_lighting.TileSizeX));
    try testing.expectEqual(@as(u32, 45), compute_helper.divRoundUp(720, hlsl_tiled_lighting.TileSizeY));
}

test "the conservative scale grows the volume and never shrinks it" {
    const fov = Vec2{ 0.6, 0.4 };
    const tile_count_inv = Vec2{ 1.0 / 80.0, 1.0 / 45.0 };

    // A light far away subtends a small angle, so it needs proportionally more
    // padding to cover the tiles it clips.
    const near = computeAabbConservativeScale(.{ 1, 1, 1 }, 2.0, fov, tile_count_inv);
    const far = computeAabbConservativeScale(.{ 1, 1, 1 }, 200.0, fov, tile_count_inv);

    try testing.expect(near >= 1.0);
    try testing.expect(far >= 1.0);
    try testing.expect(far > near);
}

test "a zero-distance light does not divide by zero" {
    // distance_to_camera is clamped by an epsilon, so a light sitting exactly
    // on the camera still produces a finite scale.
    const scale = computeAabbConservativeScale(.{ 1, 1, 1 }, 0.0, .{ 0.6, 0.4 }, .{ 0.0125, 0.022 });

    try testing.expect(std.math.isFinite(scale));
    try testing.expect(scale >= 1.0);
}
