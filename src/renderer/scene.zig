// A minimal scene for the M4a/M4b gates.
//
// The real scene — procedural track, player ship, chase camera, three lights —
// is M5. This exists so the culling and forward passes have something to draw
// before trackgen and the scene graph setup land, which is the only way to tell
// whether they work.
//
// The asset side already matches GameLoop.cpp: the four default PNG maps every
// untextured mesh falls back to, then the SciFiHelmet glTF with its four DDS
// maps and the material table built from the document.

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
const vma = @import("vulkan/vma.zig").c;

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

pub const Scene = struct {
    graph: prepare_buckets.SceneGraph,
    mesh_allocs: std.ArrayList(mesh2.MeshAlloc) = .empty,
    camera_node: *prepare_buckets.SceneNode,

    /// Derived from the loaded meshes so the near/far planes bracket them. The
    /// assets vary wildly in scale — ship.obj is ~1500 units across and centred
    /// 151 units off the ground, while the helmet is unit-sized — so a
    /// hardcoded frustum shows nothing for most of them.
    near_plane: f32,
    far_plane: f32,

    allocator: std.mem.Allocator,

    pub fn deinit(self: *Scene, allocator: std.mem.Allocator) void {
        self.mesh_allocs.deinit(self.allocator);
        self.graph.deinit(allocator);
    }
};

pub fn createPlaceholderScene(
    allocator: std.mem.Allocator,
    io: std.Io,
    vkd: anytype,
    device: vk.Device,
    mesh_cache: *mesh_cache_module.MeshCache,
    material_resources: *material_resources_module.MaterialResources,
    vma_instance: vma.VmaAllocator,
    mesh_path: []const u8,
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

        try material_resources.loadDdsTextures(
            vkd,
            device,
            vma_instance,
            io,
            image_paths,
            gltf_texture_span,
        );
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
    const obj_data = try std.Io.Dir.cwd().readFileAlloc(io, mesh_path, allocator, .limited(1 << 30));
    defer allocator.free(obj_data);

    var obj_mesh = try obj_loader.loadObjFromSlice(allocator, obj_data);
    defer obj_mesh.deinit(allocator);

    log.info("loaded '{s}': {} vertices, {} indices", .{
        mesh_path,
        obj_mesh.positions.items.len,
        obj_mesh.indexes.items.len,
    });

    var gltf_mesh = try gltf.loadMesh(0, allocator);
    defer gltf_mesh.deinit(allocator);

    var handles: [2]mesh2.MeshHandle = undefined;
    try mesh_cache_module.loadMeshes(
        mesh_cache,
        vma_instance,
        allocator,
        &.{ obj_mesh, gltf_mesh },
        &handles,
    );

    var mesh_allocs: std.ArrayList(mesh2.MeshAlloc) = .empty;
    errdefer mesh_allocs.deinit(allocator);

    for (mesh_cache.mesh2_instances.items) |instance| {
        try mesh_allocs.append(allocator, instance.lods_allocs[0]);
    }

    // ---- Nodes ----
    //
    // Both meshes are normalised into the world scale the engine assumes rather
    // than placed at their authored size. `default_light_projection_matrix` is
    // a hardcoded 0.1..100 frustum, so a scene the size of ship.obj (~1500
    // units across, centred 151 units up) falls entirely outside every shadow
    // map. The game scene is authored small enough for it; the placeholder has
    // to be normalised to match. M5 replaces all of this with the real player
    // node hierarchy.
    const helmet_radius = 1.0;
    const obj_radius = 3.0;

    // The helmet sits at the origin and the .obj well behind it, so the key
    // light's shadow falls from one onto the other.
    const helmet_center = linalg.Vec3{ 0, 0, 0 };
    const obj_center = linalg.Vec3{ 0, 0, -obj_radius * 2.0 };

    const obj_node = try graph.createSceneNode(
        normalizeTransform(computeBounds(obj_mesh.positions.items), obj_center, obj_radius),
        null,
    );

    try graph.scene_meshes.append(allocator, .{
        .scene_node = obj_node,
        .mesh_handle = handles[0],
        .material_handle = default_material,
    });

    const helmet_node = try graph.createSceneNode(
        normalizeTransform(computeBounds(gltf_mesh.positions.items), helmet_center, helmet_radius),
        null,
    );

    try graph.scene_meshes.append(allocator, .{
        .scene_node = helmet_node,
        .mesh_handle = handles[1],
        .material_handle = @enumFromInt(gltf_material_span.offset),
    });

    // ---- Camera ----
    //
    // Framed on the helmet, from far enough out that its bounding sphere fits
    // the vertical FOV with some margin.
    const distance = helmet_radius * 3.0;
    const eye = helmet_center + linalg.Vec3{ 0, helmet_radius * 0.3, distance };

    const camera_node = try graph.createSceneNode(
        linalg.mat4x3FromMat4(linalg.inverseMat4(
            linalg.lookAtRh(eye, helmet_center, .{ 0, 1, 0 }),
        )),
        null,
    );
    graph.camera_node = camera_node;

    // ---- Lights ----
    //
    // The key light sits in front of the helmet so its shadow falls back onto
    // the .obj behind. The three hardcoded game lights are M5.
    const light_positions = [_]linalg.Vec3{
        .{ 1.2, 1.6, 2.4 },
        .{ -2.4, 0.8, 1.2 },
        .{ 0.4, -1.8, 2.2 },
    };

    const light_colors = [_]linalg.Vec3{
        .{ 1.0, 1.0, 1.0 },
        .{ 0.6, 0.7, 1.0 },
        .{ 1.0, 0.8, 0.5 },
    };

    for (light_positions, light_colors, 0..) |position, color, i| {
        // A shadow-casting light needs its whole view matrix, not just a
        // position: the shadow pass renders through it.
        const light_node = try graph.createSceneNode(
            linalg.mat4x3FromMat4(linalg.inverseMat4(linalg.lookAtRh(position, helmet_center, .{ 0, 1, 0 }))),
            null,
        );

        try graph.scene_lights.append(allocator, .{
            .color = color,
            .intensity = 6.0,
            .radius = 20.0,
            // Only the key light casts. A shadow map per light would need one
            // culling pass each, and the culling output only has room for
            // max_meshlet_culling_pass_count of them including the main view.
            .shadow_map_size = if (i == 0) .{ 1024, 1024 } else .{ 0, 0 },
            .scene_node = light_node,
        });
    }

    return .{
        .graph = graph,
        .mesh_allocs = mesh_allocs,
        .camera_node = camera_node,
        .near_plane = 0.1,
        .far_plane = 100.0,
        .allocator = allocator,
    };
}

/// Maps a mesh's own bounding sphere onto `target_center` / `target_radius`, so
/// assets authored at wildly different scales all land in the same world.
fn normalizeTransform(bounds: Bounds, target_center: linalg.Vec3, target_radius: f32) linalg.Mat4x3 {
    const factor = target_radius / @max(bounds.radius, 1e-6);

    return linalg.mat4x3FromMat4(linalg.mulMat4(
        linalg.mulMat4(linalg.translate(target_center), linalg.scale(@splat(factor))),
        linalg.translate(-bounds.center),
    ));
}

const Bounds = struct { center: linalg.Vec3, radius: f32 };

fn computeBounds(positions: []const [3]f32) Bounds {
    if (positions.len == 0) return .{ .center = .{ 0, 0, 0 }, .radius = 1 };

    var min_p = linalg.Vec3{ positions[0][0], positions[0][1], positions[0][2] };
    var max_p = min_p;

    for (positions) |p| {
        const v = linalg.Vec3{ p[0], p[1], p[2] };
        min_p = @min(min_p, v);
        max_p = @max(max_p, v);
    }

    const center = (min_p + max_p) * linalg.Vec3{ 0.5, 0.5, 0.5 };

    var radius: f32 = 0;
    for (positions) |p| {
        const v = linalg.Vec3{ p[0], p[1], p[2] };
        radius = @max(radius, linalg.length(v - center));
    }

    return .{ .center = center, .radius = radius };
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
