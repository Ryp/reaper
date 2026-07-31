// The game scene — port of the ENABLE_GAME_SCENE + GLTF_TEST block of
// GameLoop.cpp.
//
// A hundred procedurally generated track chunks, the player node with the
// helmet mesh and the chase camera parented to it, and three lights. The
// physics sim is post-v1, so the player node keeps its initial transform
// instead of being driven by Bullet.

const std = @import("std");
const vk = @import("vulkan");

const camera_module = @import("camera.zig");
const gltf_loader = @import("../mesh/gltf_loader.zig");
const linalg = @import("../math/linalg.zig");
const mesh_module = @import("../mesh/mesh.zig");
const material_resources_module = @import("vulkan/material_resources.zig");
const mesh_cache_module = @import("vulkan/mesh_cache.zig");
const mesh2 = @import("mesh2.zig");
const obj_loader = @import("../mesh/obj_loader.zig");
const prepare_buckets = @import("prepare_buckets.zig");
const trackgen = @import("../neptune/trackgen.zig");
const vma = @import("vulkan/vma.zig").c;

const Vec3 = linalg.Vec3;

const log = std.log.scoped(.game);

/// Loaded for every scene, and what a mesh with no material of its own uses.
/// Only albedo is sRGB-encoded; roughness/normal/AO carry linear data and
/// decoding them through the sRGB EOTF would be wrong.
const default_material_textures = [_][]const u8{
    "res/texture/default_standard_material/albedo.png",
    "res/texture/default_standard_material/metalness_roughness.png",
    "res/texture/default_standard_material/normal.png",
    "res/texture/default_standard_material/ao.png",
};

const default_material_is_srgb = [_]bool{ true, false, false, false };

const gltf_path = "res/model/sci_fi_helmet/SciFiHelmet.gltf";

const track_mesh_path = "res/model/track_chunk_simple.obj";
const track_mesh_length: f32 = 10.0;

/// Public so the Physics window's sliders open on the values that actually
/// produced the track on screen.
pub const track_gen_info = trackgen.GenerationInfo{
    .chunk_count = 100,
    .radius_min_meter = 300.0,
    .radius_max_meter = 600.0,
    .chaos = 0.4,
};

/// DEVIATION: the C++ seeds from std::random_device, so its track differs every
/// run and cannot be matched. A fixed seed makes the Zig track reproducible,
/// which is what makes screenshot comparison across runs meaningful at all.
const track_seed: u64 = 0x5EED;

const up_ws = Vec3{ 0, 1, 0 };

/// The camera's near/far and every light's projection are hardcoded frusta, so
/// these are fixed rather than derived from the scene bounds.
const camera_near_plane: f32 = 0.1;
const camera_far_plane: f32 = 100.0;

pub const Scene = struct {
    graph: prepare_buckets.SceneGraph,
    mesh_allocs: std.ArrayList(mesh2.MeshAlloc) = .empty,
    camera_node: *prepare_buckets.SceneNode,
    /// The ship. The physics sim that would drive it is post-v1, so this keeps
    /// its initial transform — but the Physics window still reads it back, and
    /// wiring the sim up later means writing here and nothing else.
    player_node: *prepare_buckets.SceneNode,

    near_plane: f32 = camera_near_plane,
    far_plane: f32 = camera_far_plane,

    allocator: std.mem.Allocator,

    pub fn deinit(self: *Scene, allocator: std.mem.Allocator) void {
        self.mesh_allocs.deinit(self.allocator);
        self.graph.deinit(allocator);
    }
};

pub fn createGameScene(
    allocator: std.mem.Allocator,
    io: std.Io,
    vkd: anytype,
    device: vk.Device,
    mesh_cache: *mesh_cache_module.MeshCache,
    material_resources: *material_resources_module.MaterialResources,
    vma_instance: vma.VmaAllocator,
) !Scene {
    var graph = prepare_buckets.SceneGraph.init(allocator);
    errdefer graph.deinit(allocator);

    // ---- Default material ----
    const default_texture_span = try material_resources.allocMaterialTextures(default_material_textures.len);

    try material_resources.loadPngTextures(
        vkd,
        device,
        vma_instance,
        &default_material_textures,
        default_texture_span,
        &default_material_is_srgb,
    );

    const default_material = try graph.allocSceneMaterial(allocator);
    graph.scene_materials.items[default_material.index()] = .{
        .base_color_texture = @enumFromInt(default_texture_span.offset + 0),
        .metal_roughness_texture = @enumFromInt(default_texture_span.offset + 1),
        .normal_map_texture = @enumFromInt(default_texture_span.offset + 2),
        .ao_texture = @enumFromInt(default_texture_span.offset + 3),
    };

    // ---- glTF: helmet mesh, DDS maps, material table ----
    var gltf = try gltf_loader.load(allocator, gltf_path);
    defer gltf.deinit();

    const gltf_texture_span = try material_resources.allocMaterialTextures(gltf.imageCount());

    {
        const image_paths = try allocator.alloc([]const u8, gltf.imageCount());
        defer {
            for (image_paths) |path| allocator.free(path);
            allocator.free(image_paths);
        }

        // Loaded in document order, so an image index doubles as an offset into
        // the handle span.
        for (image_paths, 0..) |*path, i| {
            path.* = try gltf.imagePath(@intCast(i), allocator);
        }

        try material_resources.loadDdsTextures(vkd, device, vma_instance, io, image_paths, gltf_texture_span);
    }

    const gltf_material_span = try graph.allocSceneMaterials(allocator, gltf.materialCount());

    for (0..gltf.materialCount()) |i| {
        const material = try gltf.material(@intCast(i));

        graph.scene_materials.items[gltf_material_span.offset + i] = .{
            .base_color_texture = @enumFromInt(gltf_texture_span.offset + material.base_color_image),
            .metal_roughness_texture = @enumFromInt(gltf_texture_span.offset + material.metal_roughness_image),
            .normal_map_texture = @enumFromInt(gltf_texture_span.offset + material.normal_image),
            .ao_texture = @enumFromInt(gltf_texture_span.offset + material.ao_image),
        };
    }

    // ---- Meshes ----
    //
    // Every mesh the scene will ever use is loaded in one batch, because
    // MeshCache hands out allocations in load order and `mesh_allocs` is
    // indexed by MeshHandle.
    var meshes: std.ArrayList(mesh_module.Mesh) = .empty;
    defer {
        for (meshes.items) |*mesh| mesh.deinit(allocator);
        meshes.deinit(allocator);
    }

    var gltf_mesh = try gltf.loadMesh(0, allocator);
    {
        errdefer gltf_mesh.deinit(allocator);
        try meshes.append(allocator, gltf_mesh);
    }

    var track = try generateTrack(allocator, io, &meshes);
    defer track.deinit(allocator);

    const mesh_handles = try allocator.alloc(mesh2.MeshHandle, meshes.items.len);
    defer allocator.free(mesh_handles);

    try mesh_cache_module.loadMeshes(mesh_cache, vma_instance, allocator, meshes.items, mesh_handles);

    var mesh_allocs: std.ArrayList(mesh2.MeshAlloc) = .empty;
    errdefer mesh_allocs.deinit(allocator);

    for (mesh_cache.mesh2_instances.items) |instance| {
        try mesh_allocs.append(allocator, instance.lods_allocs[0]);
    }

    const helmet_mesh_handle = mesh_handles[0];
    const track_mesh_handles = mesh_handles[1..];

    // ---- Track in the scene ----
    for (track.chunk_transforms, track_mesh_handles) |chunk_transform, mesh_handle| {
        try graph.scene_meshes.append(allocator, .{
            .scene_node = try graph.createSceneNode(chunk_transform, null),
            .mesh_handle = mesh_handle,
            .material_handle = default_material,
        });
    }

    // ---- Player, camera, helmet ----
    //
    // One node carries the physics object and the mesh hangs off it as a child,
    // so the mesh's authored orientation and scale stay out of the simulated
    // transform.
    const player_initial_transform = linalg.mat4x3FromMat4(linalg.translate(.{ 1.1, 0.8, 0.0 }));
    const player_scene_node = try graph.createSceneNode(player_initial_transform, null);

    const camera_position = Vec3{ -2.0, 0.8, 0.0 };
    const camera_local_target = Vec3{ 1.0, 0.4, 0.0 };

    const camera_local_transform = linalg.mat4x3FromMat4(linalg.inverseMat4(
        linalg.lookAtRh(camera_position, camera_local_target, up_ws),
    ));

    const camera_node = try graph.createSceneNode(camera_local_transform, player_scene_node);
    graph.camera_node = camera_node;

    const mesh_local_transform = linalg.mat4x3FromMat4(linalg.mulMat4(
        linalg.scale(@splat(0.4)),
        linalg.rotate(std.math.pi * -0.5, up_ws),
    ));

    try graph.scene_meshes.append(allocator, .{
        .scene_node = try graph.createSceneNode(mesh_local_transform, player_scene_node),
        .mesh_handle = helmet_mesh_handle,
        .material_handle = @enumFromInt(gltf_material_span.offset),
    });

    // ---- Lights ----
    //
    // Values verbatim from GameLoop.cpp. The first is parented to the player so
    // it travels with the ship; the other two are static.
    const light_target_ws = Vec3{ 0, 0, 0 };

    {
        const light_position_ws = Vec3{ -1.0, 0.0, 0.0 };
        const light_transform = linalg.mat4x3FromMat4(linalg.mulMat4(
            linalg.translate(.{ 2.0, 0.0, 0.0 }),
            linalg.inverseMat4(linalg.lookAtRh(light_position_ws, light_target_ws, up_ws)),
        ));

        try graph.scene_lights.append(allocator, .{
            .color = .{ 0.03, 0.21, 0.61 },
            .intensity = 20.0,
            .radius = 42.0,
            .shadow_map_size = .{ 1024, 1024 },
            .scene_node = try graph.createSceneNode(light_transform, player_scene_node),
        });
    }

    {
        const light_position_ws = Vec3{ 3.0, 3.0, 3.0 };
        const light_transform = linalg.mat4x3FromMat4(linalg.inverseMat4(
            linalg.lookAtRh(light_position_ws, light_target_ws, up_ws),
        ));

        try graph.scene_lights.append(allocator, .{
            .color = .{ 1.0, 1.0, 1.0 },
            .intensity = 16.0,
            .radius = 42.0,
            .shadow_map_size = .{ 512, 512 },
            .scene_node = try graph.createSceneNode(light_transform, null),
        });
    }

    {
        const light_position_ws = Vec3{ 0.0, 3.0, -3.0 };
        const light_transform = linalg.mat4x3FromMat4(linalg.inverseMat4(
            linalg.lookAtRh(light_position_ws, light_target_ws, up_ws),
        ));

        try graph.scene_lights.append(allocator, .{
            .color = .{ 0.03, 0.8, 0.21 },
            .intensity = 6.0,
            .radius = 42.0,
            .shadow_map_size = .{ 256, 256 },
            .scene_node = try graph.createSceneNode(light_transform, null),
        });
    }

    log.info("scene: {} meshes, {} lights, {} materials", .{
        graph.scene_meshes.items.len,
        graph.scene_lights.items.len,
        graph.scene_materials.items.len,
    });

    return .{
        .graph = graph,
        .mesh_allocs = mesh_allocs,
        .camera_node = camera_node,
        .player_node = player_scene_node,
        .allocator = allocator,
    };
}

/// glm's `fmat4x3[3]`, which is the translation column.
pub fn getPlayerTranslation(scene: *const Scene) [3]f32 {
    return prepare_buckets.getSceneNodeTransformSlow(scene.player_node).c[3];
}

const Track = struct {
    skeleton_nodes: []trackgen.TrackSkeletonNode,
    skinning: []trackgen.TrackSkinning,
    /// Each chunk's mesh-to-world frame; the mesh itself is skinned into that
    /// frame's local space.
    chunk_transforms: []linalg.Mat4x3,

    fn deinit(self: *Track, allocator: std.mem.Allocator) void {
        allocator.free(self.skeleton_nodes);
        allocator.free(self.skinning);
        allocator.free(self.chunk_transforms);
        self.* = undefined;
    }
};

/// Generates the skeleton and appends one skinned copy of the chunk mesh per
/// node to `meshes`. Ownership of the appended meshes passes to the caller.
fn generateTrack(
    allocator: std.mem.Allocator,
    io: std.Io,
    meshes: *std.ArrayList(mesh_module.Mesh),
) !Track {
    const chunk_count = track_gen_info.chunk_count;

    const skeleton_nodes = try allocator.alloc(trackgen.TrackSkeletonNode, chunk_count);
    errdefer allocator.free(skeleton_nodes);

    var prng = std.Random.DefaultPrng.init(track_seed);
    try trackgen.generateTrackSkeleton(track_gen_info, skeleton_nodes, prng.random());

    const skinning = try allocator.alloc(trackgen.TrackSkinning, chunk_count);
    errdefer allocator.free(skinning);

    trackgen.generateTrackSkinning(skeleton_nodes, skinning);

    const chunk_transforms = try allocator.alloc(linalg.Mat4x3, chunk_count);
    errdefer allocator.free(chunk_transforms);

    const track_data = try std.Io.Dir.cwd().readFileAlloc(io, track_mesh_path, allocator, .limited(1 << 30));
    defer allocator.free(track_data);

    var unskinned = try obj_loader.loadObjFromSlice(allocator, track_data);
    defer unskinned.deinit(allocator);

    log.info("loaded '{s}': {} vertices, {} indices", .{
        track_mesh_path,
        unskinned.positions.items.len,
        unskinned.indexes.items.len,
    });

    try meshes.ensureUnusedCapacity(allocator, chunk_count);

    for (skeleton_nodes, skinning, chunk_transforms) |node, chunk_skinning, *transform| {
        transform.* = node.in_transform_ms_to_ws;

        var chunk_mesh = try duplicateMesh(allocator, unskinned);
        errdefer chunk_mesh.deinit(allocator);

        trackgen.skinTrackChunkMesh(node, chunk_skinning, chunk_mesh.positions.items, track_mesh_length);

        meshes.appendAssumeCapacity(chunk_mesh);
    }

    return .{
        .skeleton_nodes = skeleton_nodes,
        .skinning = skinning,
        .chunk_transforms = chunk_transforms,
    };
}

fn duplicateMesh(allocator: std.mem.Allocator, source: mesh_module.Mesh) !mesh_module.Mesh {
    var copy = mesh_module.Mesh{};
    errdefer copy.deinit(allocator);

    try copy.indexes.appendSlice(allocator, source.indexes.items);
    try copy.positions.appendSlice(allocator, source.positions.items);
    try copy.attributes.appendSlice(allocator, source.attributes.items);

    return copy;
}

pub fn buildCamera(scene: *const Scene, extent: vk.Extent2D) camera_module.RendererPerspectiveCamera {
    const viewport = camera_module.buildRendererViewport(.{ extent.width, extent.height });

    const projection = camera_module.buildRendererPerspectiveProjection(
        viewport.aspect_ratio,
        scene.near_plane,
        scene.far_plane,
        std.math.degreesToRadians(45.0),
        @import("vulkan/renderpass/constants.zig").main_pass_use_reverse_z,
    );

    return camera_module.buildRendererPerspectiveCamera(
        prepare_buckets.getSceneNodeTransformSlow(scene.camera_node),
        projection,
        viewport,
    );
}
