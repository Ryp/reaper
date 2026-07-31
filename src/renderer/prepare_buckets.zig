// Port of src/renderer/PrepareBuckets.{h,cpp}
//
// Walks the scene graph once per frame and flattens it into the flat arrays the
// GPU passes consume: per-instance matrices, cull commands, light properties,
// shadow instances. Everything here is CPU-side and frame-lifetime, so it is
// arena-allocated.

const std = @import("std");

const hlsl = @import("hlsl/types.zig");
const hlsl_forward = @import("hlsl/forward.zig");
const hlsl_lighting = @import("hlsl/lighting.zig");
const hlsl_mesh_instance = @import("hlsl/mesh_instance.zig");
const hlsl_mesh_material = @import("hlsl/mesh_material.zig");
const hlsl_meshlet_culling = @import("hlsl/meshlet/meshlet_culling.zig");
const hlsl_shadow = @import("hlsl/shadow/shadow_map_pass.zig");
const hlsl_sound = @import("hlsl/sound/sound.zig");
const hlsl_tiled_lighting = @import("hlsl/tiled_lighting/tiled_lighting.zig");

const camera_module = @import("camera.zig");
const linalg = @import("../math/linalg.zig");
const mesh2 = @import("mesh2.zig");

const Mat4 = linalg.Mat4;
const Mat4x3 = linalg.Mat4x3;
const MeshAlloc = mesh2.MeshAlloc;
const MeshHandle = mesh2.MeshHandle;
const TextureHandle = mesh2.TextureHandle;

/// From renderpass/ShadowConstants.h
pub const shadow_use_reverse_z = true;

// --------------------------------------------------------------------------
// Scene graph
// --------------------------------------------------------------------------

pub const SceneNode = struct {
    /// Local space to parent space
    transform_matrix: Mat4x3,
    /// If no parent, parent space is world space
    parent: ?*SceneNode = null,
};

pub const SceneMaterial = struct {
    base_color_texture: TextureHandle = .invalid,
    metal_roughness_texture: TextureHandle = .invalid,
    normal_map_texture: TextureHandle = .invalid,
    ao_texture: TextureHandle = .invalid,
};

pub const SceneMaterialHandle = enum(u32) {
    invalid = 0xFFFF_FFFF,
    _,

    pub fn index(self: SceneMaterialHandle) u32 {
        return @intFromEnum(self);
    }
};

pub const SceneMesh = struct {
    scene_node: *SceneNode,
    mesh_handle: MeshHandle,
    material_handle: SceneMaterialHandle,
};

pub const SceneLight = struct {
    projection_matrix: Mat4 = Mat4.identity,
    color: linalg.Vec3,
    intensity: f32,
    radius: f32,
    scene_node: *SceneNode,
    /// Set to zero to disable shadow
    shadow_map_size: linalg.UVec2 = .{ 0, 0 },
};

pub const SceneGraph = struct {
    camera_node: ?*SceneNode = null,
    scene_meshes: std.ArrayList(SceneMesh) = .empty,
    scene_materials: std.ArrayList(SceneMaterial) = .empty,
    scene_lights: std.ArrayList(SceneLight) = .empty,

    /// The C++ new/deletes nodes individually with a FIXME about pooling them.
    /// An arena is that pool: nodes live as long as the scene does.
    node_arena: std.heap.ArenaAllocator,

    pub fn init(allocator: std.mem.Allocator) SceneGraph {
        return .{ .node_arena = .init(allocator) };
    }

    pub fn deinit(self: *SceneGraph, allocator: std.mem.Allocator) void {
        self.scene_meshes.deinit(allocator);
        self.scene_materials.deinit(allocator);
        self.scene_lights.deinit(allocator);
        self.node_arena.deinit();
    }

    pub fn createSceneNode(self: *SceneGraph, transform_matrix: Mat4x3, parent: ?*SceneNode) !*SceneNode {
        const node = try self.node_arena.allocator().create(SceneNode);
        node.* = .{ .transform_matrix = transform_matrix, .parent = parent };
        return node;
    }

    pub fn allocSceneMaterial(self: *SceneGraph, allocator: std.mem.Allocator) !SceneMaterialHandle {
        const old_size: u32 = @intCast(self.scene_materials.items.len);
        try self.scene_materials.append(allocator, .{});
        return @enumFromInt(old_size);
    }

    pub fn allocSceneMaterials(
        self: *SceneGraph,
        allocator: std.mem.Allocator,
        count: u32,
    ) !mesh2.HandleSpan(SceneMaterialHandle) {
        const old_size: u32 = @intCast(self.scene_materials.items.len);
        try self.scene_materials.appendNTimes(allocator, .{}, count);
        return .{ .offset = old_size, .count = count };
    }
};

/// FIXME Support proper parenting with caching and disallow cycles!
pub fn getSceneNodeTransformSlow(node: *const SceneNode) Mat4x3 {
    var accum = linalg.mat4FromMat4x3(node.transform_matrix);
    var current = node;

    while (current.parent) |parent| {
        accum = linalg.mulMat4(linalg.mat4FromMat4x3(parent.transform_matrix), accum);
        current = parent;
    }

    return linalg.mat4x3FromMat4(accum);
}

// --------------------------------------------------------------------------
// Prepared data
// --------------------------------------------------------------------------

pub const CullCmd = struct {
    mesh_alloc: MeshAlloc = .{},
    push_constants: hlsl_meshlet_culling.CullMeshletPushConstants = .{},
    instance_count: u32 = 0,
};

pub const CullPassData = struct {
    pass_index: u32,
    output_size_ts: linalg.Vec2,
    main_pass: bool,

    cull_commands: std.ArrayList(CullCmd) = .empty,
};

pub const ShadowPassData = struct {
    pass_index: u32,
    instance_offset: u32,
    instance_count: u32 = 0,
    culling_pass_index: u32,
    shadow_map_size: linalg.UVec2,
};

pub const PreparedData = struct {
    cull_passes: std.ArrayList(CullPassData) = .empty,
    cull_mesh_instance_params: std.ArrayList(hlsl_meshlet_culling.CullMeshInstanceParams) = .empty,

    mesh_instances: std.ArrayList(hlsl_mesh_instance.MeshInstance) = .empty,
    mesh_materials: std.ArrayList(hlsl_mesh_material.MeshMaterial) = .empty,

    main_culling_pass_index: u32 = 0,
    forward_pass_constants: hlsl_forward.ForwardPassParams = .{},

    point_lights: std.ArrayList(hlsl_lighting.PointLightProperties) = .empty,
    tiled_light_constants: hlsl_tiled_lighting.TiledLightingConstants = .{},

    shadow_passes: std.ArrayList(ShadowPassData) = .empty,
    shadow_instance_params: std.ArrayList(hlsl_shadow.ShadowMapInstanceParams) = .empty,

    audio_push_constants: hlsl_sound.SoundPushConstants = .{},
    audio_instance_params: std.ArrayList(hlsl_sound.OscillatorInstance) = .empty,
};

// --------------------------------------------------------------------------
// Light projection
// --------------------------------------------------------------------------

fn note(semitone_offset: f32, base_freq: f32) f32 {
    return base_freq * std.math.pow(f32, 2.0, semitone_offset / 12.0);
}

fn applyReverseZFixup(projection_matrix: Mat4, reverse_z: bool) Mat4 {
    if (!reverse_z) return projection_matrix;

    // NOTE: we might want to do it by hand to limit precision loss
    var reverse_z_transform = Mat4.identity;
    reverse_z_transform.c[3][2] = 1.0;
    reverse_z_transform.c[2][2] = -1.0;

    return linalg.mulMat4(reverse_z_transform, projection_matrix);
}

fn buildPerspectiveMatrix(near_plane: f32, far_plane: f32, aspect_ratio: f32, fov_radian: f32) Mat4 {
    var projection = linalg.perspectiveRhZo(fov_radian, aspect_ratio, near_plane, far_plane);

    // Flip viewport Y
    projection.c[1] = -projection.c[1];

    return projection;
}

/// NOTE: duplicated from Camera.cpp on the C++ side too — the two copies are
/// independent there, so they stay independent here.
pub fn defaultLightProjectionMatrix() Mat4 {
    return applyReverseZFixup(
        buildPerspectiveMatrix(0.1, 100.0, 1.0, std.math.pi * 0.25),
        shadow_use_reverse_z,
    );
}

// --------------------------------------------------------------------------
// prepare_scene
// --------------------------------------------------------------------------

fn insertCullCommand(
    allocator: std.mem.Allocator,
    cull_pass: *CullPassData,
    mesh_alloc: MeshAlloc,
    cull_instance_index_start: u32,
    cull_instance_count: u32,
) !void {
    std.debug.assert(mesh_alloc.index_count % 3 == 0);

    try cull_pass.cull_commands.append(allocator, .{
        .instance_count = cull_instance_count,
        .push_constants = .{
            .meshlet_offset = mesh_alloc.meshlet_offset,
            .meshlet_count = mesh_alloc.meshlet_count,
            .first_index = mesh_alloc.index_offset,
            .first_vertex = mesh_alloc.position_offset,
            .cull_instance_offset = cull_instance_index_start,
        },
    });
}

/// `mesh_allocs` is indexed by MeshHandle and supplies the LOD 0 allocation for
/// each mesh — the C++ reaches into MeshCache::mesh2_instances for this, but
/// nothing else here needs the cache, so only what is used is passed in.
pub fn prepareScene(
    allocator: std.mem.Allocator,
    scene: *const SceneGraph,
    prepared: *PreparedData,
    mesh_allocs: []const MeshAlloc,
    main_camera: camera_module.RendererPerspectiveCamera,
    current_audio_frame: u32,
) !void {
    // ---- Shadow passes ----
    for (scene.scene_lights.items) |light| {
        if (light.shadow_map_size[0] == 0 and light.shadow_map_size[1] == 0) continue;

        const shadow_pass_index: u32 = @intCast(prepared.shadow_passes.items.len);
        const cull_pass_index: u32 = @intCast(prepared.cull_passes.items.len);

        try prepared.cull_passes.append(allocator, .{
            .pass_index = cull_pass_index,
            .output_size_ts = .{
                @floatFromInt(light.shadow_map_size[0]),
                @floatFromInt(light.shadow_map_size[1]),
            },
            .main_pass = false,
        });

        try prepared.shadow_passes.append(allocator, .{
            .pass_index = shadow_pass_index,
            .instance_offset = @intCast(prepared.shadow_instance_params.items.len),
            .culling_pass_index = cull_pass_index,
            .shadow_map_size = light.shadow_map_size,
        });

        const cull_pass = &prepared.cull_passes.items[cull_pass_index];

        const light_transform = getSceneNodeTransformSlow(light.scene_node);
        const light_transform_inv = linalg.mat4x3FromMat4(
            linalg.inverseMat4(linalg.mat4FromMat4x3(light_transform)),
        );
        const light_projection_matrix = defaultLightProjectionMatrix();
        const light_view_proj_matrix = linalg.mulMat4(
            light_projection_matrix,
            linalg.mat4FromMat4x3(light_transform_inv),
        );

        for (scene.scene_meshes.items, 0..) |scene_mesh, i| {
            const mesh_transform = getSceneNodeTransformSlow(scene_mesh.scene_node);
            const ms_to_cs_matrix = linalg.mulMat4(
                light_view_proj_matrix,
                linalg.mat4FromMat4x3(mesh_transform),
            );

            try prepared.shadow_instance_params.append(allocator, .{
                .ms_to_cs_matrix = hlsl.float4x4(ms_to_cs_matrix),
            });

            const cull_instance_index: u32 = @intCast(prepared.cull_mesh_instance_params.items.len);

            const ms_to_vs_matrix = linalg.mulMat4(
                linalg.mat4FromMat4x3(light_transform_inv),
                linalg.mat4FromMat4x3(mesh_transform),
            );
            const vs_to_ms_matrix = linalg.inverseMat4(ms_to_vs_matrix);
            const vs_to_ms_translate = linalg.mulMat4Vec4(vs_to_ms_matrix, .{ 0, 0, 0, 1 });

            try prepared.cull_mesh_instance_params.append(allocator, .{
                .ms_to_cs_matrix = hlsl.float4x4(ms_to_cs_matrix),
                .vs_to_ms_matrix_translate = hlsl.float3(linalg.xyz(vs_to_ms_translate)),
                .instance_id = @intCast(i),
            });

            try insertCullCommand(
                allocator,
                cull_pass,
                mesh_allocs[scene_mesh.mesh_handle.index()],
                cull_instance_index,
                1,
            );
        }

        // Count instances we just inserted
        const shadow_total_instance_count: u32 = @intCast(prepared.shadow_instance_params.items.len);
        const shadow_pass = &prepared.shadow_passes.items[shadow_pass_index];
        shadow_pass.instance_count = shadow_total_instance_count - shadow_pass.instance_offset;
    }

    // ---- Main + culling pass ----
    prepared.forward_pass_constants.ws_to_vs_matrix = hlsl.float3x4(main_camera.ws_to_vs_matrix);
    prepared.forward_pass_constants.ws_to_cs_matrix = hlsl.float4x4(main_camera.ws_to_cs_matrix);
    prepared.forward_pass_constants.point_light_count = @intCast(scene.scene_lights.items.len);

    prepared.tiled_light_constants.cs_to_vs = hlsl.float4x4(main_camera.perspective_projection.cs_to_vs_matrix);
    prepared.tiled_light_constants.vs_to_ws = hlsl.float3x4(main_camera.vs_to_ws_matrix);

    for (scene.scene_lights.items, 0..) |light, scene_light_index| {
        const light_transform = getSceneNodeTransformSlow(light.scene_node);
        const light_transform_inv = linalg.mat4x3FromMat4(
            linalg.inverseMat4(linalg.mat4FromMat4x3(light_transform)),
        );

        const light_position_ws = linalg.mulMat4Vec4(
            linalg.mat4FromMat4x3(light_transform),
            .{ 0, 0, 0, 1 },
        );
        const light_position_vs = linalg.mulMat4Vec4(
            linalg.mat4FromMat4x3(main_camera.ws_to_vs_matrix),
            linalg.vec4FromVec3(linalg.xyz(light_position_ws), 1),
        );

        const light_view_proj_matrix = linalg.mulMat4(
            defaultLightProjectionMatrix(),
            linalg.mat4FromMat4x3(light_transform_inv),
        );

        const has_shadow = !(light.shadow_map_size[0] == 0 and light.shadow_map_size[1] == 0);

        try prepared.point_lights.append(allocator, .{
            .light_ws_to_cs = hlsl.float4x4(light_view_proj_matrix),
            .position_vs = hlsl.float3(linalg.xyz(light_position_vs)),
            .intensity = light.intensity,
            .color = hlsl.float3(light.color),
            .radius_sq = light.radius * light.radius,
            .shadow_map_index = if (has_shadow)
                @intCast(scene_light_index) // FIXME
            else
                hlsl_lighting.InvalidShadowMapIndex,
        });
    }

    {
        const cull_pass_index: u32 = @intCast(prepared.cull_passes.items.len);

        try prepared.cull_passes.append(allocator, .{
            .pass_index = cull_pass_index,
            .output_size_ts = .{
                @floatFromInt(main_camera.viewport.extent[0]),
                @floatFromInt(main_camera.viewport.extent[1]),
            },
            .main_pass = true,
        });

        prepared.main_culling_pass_index = cull_pass_index;

        const cull_pass = &prepared.cull_passes.items[cull_pass_index];

        try prepared.mesh_materials.ensureTotalCapacity(allocator, scene.scene_materials.items.len);

        for (scene.scene_materials.items) |scene_material| {
            prepared.mesh_materials.appendAssumeCapacity(.{
                .albedo_texture_index = scene_material.base_color_texture.index(),
                .roughness_texture_index = scene_material.metal_roughness_texture.index(),
                .normal_texture_index = scene_material.normal_map_texture.index(),
                .ao_texture_index = scene_material.ao_texture.index(),
            });
        }

        try prepared.mesh_instances.ensureTotalCapacity(allocator, scene.scene_meshes.items.len);

        for (scene.scene_meshes.items, 0..) |scene_mesh, i| {
            const mesh_transform = getSceneNodeTransformSlow(scene_mesh.scene_node);

            // Assumption that our 3x3 submatrix is orthonormal (no skew /
            // non-uniform scaling). FIXME use 4x3 matrices directly
            const ms_to_vs_matrix = linalg.mulMat4(
                linalg.mat4FromMat4x3(main_camera.ws_to_vs_matrix),
                linalg.mat4FromMat4x3(mesh_transform),
            );

            const ms_to_cs_matrix = linalg.mulMat4(
                main_camera.ws_to_cs_matrix,
                linalg.mat4FromMat4x3(mesh_transform),
            );

            prepared.mesh_instances.appendAssumeCapacity(.{
                .ms_to_cs_matrix = hlsl.float4x4(ms_to_cs_matrix),
                .ms_to_ws_matrix = hlsl.float3x4(mesh_transform),
                .normal_ms_to_vs_matrix = hlsl.float3x3(linalg.mat3FromMat4(ms_to_vs_matrix)),
                .material_index = scene_mesh.material_handle.index(),
            });

            const cull_instance_index: u32 = @intCast(prepared.cull_mesh_instance_params.items.len);

            const vs_to_ms_matrix = linalg.inverseMat4(ms_to_vs_matrix);
            const vs_to_ms_translate = linalg.mulMat4Vec4(vs_to_ms_matrix, .{ 0, 0, 0, 1 });

            try prepared.cull_mesh_instance_params.append(allocator, .{
                .ms_to_cs_matrix = hlsl.float4x4(ms_to_cs_matrix),
                .vs_to_ms_matrix_translate = hlsl.float3(linalg.xyz(vs_to_ms_translate)),
                .instance_id = @intCast(i),
            });

            try insertCullCommand(
                allocator,
                cull_pass,
                mesh_allocs[scene_mesh.mesh_handle.index()],
                cull_instance_index,
                1,
            );
        }
    }

    // ---- Audio pass ----
    prepared.audio_push_constants.start_sample = current_audio_frame;

    for (0..hlsl_sound.OscillatorCount) |i| {
        const index: f32 = @floatFromInt(i);

        try prepared.audio_instance_params.append(allocator, .{
            .frequency = note(0.0 + index * 5.0, 440.0),
            .pan = -0.60 + index * 0.4,
        });
    }
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "scene node transforms compose up the parent chain" {
    var scene = SceneGraph.init(testing.allocator);
    defer scene.deinit(testing.allocator);

    const parent = try scene.createSceneNode(linalg.mat4x3FromMat4(linalg.translate(.{ 1, 0, 0 })), null);
    const child = try scene.createSceneNode(linalg.mat4x3FromMat4(linalg.translate(.{ 0, 2, 0 })), parent);
    const grandchild = try scene.createSceneNode(linalg.mat4x3FromMat4(linalg.translate(.{ 0, 0, 3 })), child);

    const transform = getSceneNodeTransformSlow(grandchild);

    // Translations accumulate through the chain.
    try testing.expectEqual(linalg.Vec3{ 1, 2, 3 }, transform.c[3]);

    // A node with no parent is its own transform.
    try testing.expectEqual(linalg.Vec3{ 1, 0, 0 }, getSceneNodeTransformSlow(parent).c[3]);
}

test "prepare_scene emits one cull command per mesh per pass" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    var scene = SceneGraph.init(testing.allocator);
    defer scene.deinit(testing.allocator);

    const mesh_allocs = [_]MeshAlloc{.{
        .index_count = 3,
        .vertex_count = 3,
        .meshlet_count = 1,
    }};

    // Two meshes, one shadow-casting light and one without a shadow map.
    for (0..2) |_| {
        const node = try scene.createSceneNode(Mat4x3.identity, null);
        try scene.scene_meshes.append(testing.allocator, .{
            .scene_node = node,
            .mesh_handle = @enumFromInt(0),
            .material_handle = @enumFromInt(0),
        });
    }

    const shadow_light_node = try scene.createSceneNode(Mat4x3.identity, null);
    try scene.scene_lights.append(testing.allocator, .{
        .color = .{ 1, 1, 1 },
        .intensity = 1.0,
        .radius = 10.0,
        .scene_node = shadow_light_node,
        .shadow_map_size = .{ 512, 512 },
    });

    const plain_light_node = try scene.createSceneNode(Mat4x3.identity, null);
    try scene.scene_lights.append(testing.allocator, .{
        .color = .{ 1, 0, 0 },
        .intensity = 2.0,
        .radius = 5.0,
        .scene_node = plain_light_node,
    });

    try scene.scene_materials.append(testing.allocator, .{});

    const projection = camera_module.buildRendererPerspectiveProjection(
        16.0 / 9.0,
        0.1,
        100.0,
        std.math.degreesToRadians(45.0),
        true,
    );
    const camera = camera_module.buildRendererPerspectiveCamera(
        Mat4x3.identity,
        projection,
        camera_module.buildRendererViewport(.{ 1280, 720 }),
    );

    var prepared = PreparedData{};
    try prepareScene(allocator, &scene, &prepared, &mesh_allocs, camera, 0);

    // One shadow pass (only the light with a shadow map) plus the main pass.
    try testing.expectEqual(@as(usize, 1), prepared.shadow_passes.items.len);
    try testing.expectEqual(@as(usize, 2), prepared.cull_passes.items.len);
    try testing.expectEqual(@as(u32, 1), prepared.main_culling_pass_index);
    try testing.expect(prepared.cull_passes.items[1].main_pass);
    try testing.expect(!prepared.cull_passes.items[0].main_pass);

    // Each pass issues one cull command per mesh.
    for (prepared.cull_passes.items) |cull_pass| {
        try testing.expectEqual(@as(usize, 2), cull_pass.cull_commands.items.len);
    }

    // The shadow pass covers both meshes.
    try testing.expectEqual(@as(u32, 0), prepared.shadow_passes.items[0].instance_offset);
    try testing.expectEqual(@as(u32, 2), prepared.shadow_passes.items[0].instance_count);

    // Cull instances: two per pass, main pass last.
    try testing.expectEqual(@as(usize, 4), prepared.cull_mesh_instance_params.items.len);
    try testing.expectEqual(@as(usize, 2), prepared.mesh_instances.items.len);

    // Both lights become point lights; only the first has a shadow map index.
    try testing.expectEqual(@as(usize, 2), prepared.point_lights.items.len);
    try testing.expectEqual(@as(u32, 0), prepared.point_lights.items[0].shadow_map_index);
    try testing.expectEqual(
        hlsl_lighting.InvalidShadowMapIndex,
        prepared.point_lights.items[1].shadow_map_index,
    );

    // radius_sq really is the square.
    try testing.expectEqual(@as(f32, 100.0), prepared.point_lights.items[0].radius_sq);
    try testing.expectEqual(@as(f32, 25.0), prepared.point_lights.items[1].radius_sq);

    try testing.expectEqual(
        @as(u32, 2),
        prepared.forward_pass_constants.point_light_count,
    );

    // The audio pass always emits one instance per oscillator.
    try testing.expectEqual(
        @as(usize, hlsl_sound.OscillatorCount),
        prepared.audio_instance_params.items.len,
    );
}

test "the light projection uses reverse z" {
    const projection = defaultLightProjectionMatrix();

    // Near maps to 1, far to 0 — the shadow map is reverse-z.
    const on_near = linalg.mulMat4Vec4(projection, .{ 0, 0, -0.1, 1 });
    try testing.expectApproxEqAbs(@as(f32, 1), on_near[2] / on_near[3], 1e-4);

    const on_far = linalg.mulMat4Vec4(projection, .{ 0, 0, -100.0, 1 });
    try testing.expectApproxEqAbs(@as(f32, 0), on_far[2] / on_far[3], 1e-4);
}
