// A minimal scene for the M4a gate.
//
// The real scene — procedural track, player ship, chase camera, three lights —
// is M5. This exists so the culling and forward passes have something to draw
// before trackgen and the scene graph setup land, which is the only way to tell
// whether they work.

const std = @import("std");
const vk = @import("vulkan");

const camera_module = @import("camera.zig");
const linalg = @import("../math/linalg.zig");
const mesh_module = @import("../mesh/mesh.zig");
const material_resources_module = @import("vulkan/material_resources.zig");
const mesh_cache_module = @import("vulkan/mesh_cache.zig");
const mesh2 = @import("mesh2.zig");
const obj_loader = @import("../mesh/obj_loader.zig");
const prepare_buckets = @import("prepare_buckets.zig");
const vma = @import("vulkan/vma.zig").c;

const log = std.log.scoped(.game);

pub const Scene = struct {
    graph: prepare_buckets.SceneGraph,
    mesh_allocs: std.ArrayList(mesh2.MeshAlloc) = .empty,
    camera_node: *prepare_buckets.SceneNode,

    /// Derived from the loaded mesh so the near/far planes bracket it. The
    /// assets vary wildly in scale — ship.obj is ~1500 units across and centred
    /// 151 units off the ground, while others are unit-sized — so a hardcoded
    /// frustum shows nothing for most of them.
    near_plane: f32,
    far_plane: f32,

    allocator: std.mem.Allocator,

    pub fn deinit(self: *Scene, allocator: std.mem.Allocator) void {
        self.mesh_allocs.deinit(self.allocator);
        self.graph.deinit(allocator);
    }
};

/// Loads one mesh and puts it in front of a fixed camera with a single light.
/// The four maps the forward shader samples, in the order MeshMaterial expects.
/// Only base colour is sRGB; the rest carry linear data.
pub const MaterialTextureSet = struct {
    base_color: []const u8,
    metal_roughness: []const u8,
    normal: []const u8,
    ao: []const u8,
};

pub const sci_fi_helmet_textures = MaterialTextureSet{
    .base_color = "res/model/sci_fi_helmet/SciFiHelmet_BaseColor.png",
    .metal_roughness = "res/model/sci_fi_helmet/SciFiHelmet_MetallicRoughness.png",
    .normal = "res/model/sci_fi_helmet/SciFiHelmet_Normal.png",
    .ao = "res/model/sci_fi_helmet/SciFiHelmet_AmbientOcclusion.png",
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

    // ---- Mesh ----
    const data = try std.Io.Dir.cwd().readFileAlloc(io, mesh_path, allocator, .limited(1 << 30));
    defer allocator.free(data);

    var mesh = try obj_loader.loadObjFromSlice(allocator, data);
    defer mesh.deinit(allocator);

    log.info("loaded '{s}': {} vertices, {} indices", .{
        mesh_path,
        mesh.positions.items.len,
        mesh.indexes.items.len,
    });

    var handles: [1]mesh2.MeshHandle = undefined;
    try mesh_cache_module.loadMeshes(mesh_cache, vma_instance, allocator, &.{mesh}, &handles);

    var mesh_allocs: std.ArrayList(mesh2.MeshAlloc) = .empty;
    errdefer mesh_allocs.deinit(allocator);

    for (mesh_cache.mesh2_instances.items) |instance| {
        try mesh_allocs.append(allocator, instance.lods_allocs[0]);
    }

    // ---- Nodes ----
    const mesh_node = try graph.createSceneNode(linalg.Mat4x3.identity, null);

    try graph.scene_meshes.append(allocator, .{
        .scene_node = mesh_node,
        .mesh_handle = handles[0],
        .material_handle = @enumFromInt(0),
    });

    // ---- Material ----
    const textures = sci_fi_helmet_textures;

    const handle_span = try material_resources.allocMaterialTextures(4);

    try material_resources.loadPngTextures(
        vkd,
        device,
        vma_instance,
        &.{ textures.base_color, textures.metal_roughness, textures.normal, textures.ao },
        handle_span,
        // Only base colour is sRGB-encoded; roughness/normal/AO are linear data
        // and decoding them through the sRGB EOTF would be wrong.
        &.{ true, false, false, false },
    );

    const material_handle = try graph.allocSceneMaterial(allocator);
    graph.scene_materials.items[material_handle.index()] = .{
        .base_color_texture = @enumFromInt(handle_span.offset + 0),
        .metal_roughness_texture = @enumFromInt(handle_span.offset + 1),
        .normal_map_texture = @enumFromInt(handle_span.offset + 2),
        .ao_texture = @enumFromInt(handle_span.offset + 3),
    };

    // A light off to one side, no shadow map — shadows are M4b.
    const light_node = try graph.createSceneNode(
        linalg.mat4x3FromMat4(linalg.translate(.{ 3, 3, 3 })),
        null,
    );

    try graph.scene_lights.append(allocator, .{
        .color = .{ 1, 1, 1 },
        .intensity = 8.0,
        .radius = 20.0,
        .scene_node = light_node,
    });

    // ---- Camera ----
    const bounds = computeBounds(mesh.positions.items);
    const radius = @max(bounds.radius, 1e-3);

    // Pull back far enough that the bounding sphere fits the vertical FOV, with
    // some margin, and look slightly down at the centre.
    const distance = radius * 2.5;
    const eye = bounds.center + linalg.Vec3{ 0, radius * 0.35, distance };

    const camera_node = try graph.createSceneNode(
        linalg.mat4x3FromMat4(linalg.inverseMat4(
            linalg.lookAtRh(eye, bounds.center, .{ 0, 1, 0 }),
        )),
        null,
    );
    graph.camera_node = camera_node;

    log.info("mesh bounds: centre ({d:.2}, {d:.2}, {d:.2}) radius {d:.2}", .{
        bounds.center[0], bounds.center[1], bounds.center[2], radius,
    });

    // Put the light where it can actually reach the mesh.
    graph.scene_lights.items[0].scene_node.transform_matrix =
        linalg.mat4x3FromMat4(linalg.translate(bounds.center + linalg.Vec3{ radius, radius, radius }));
    graph.scene_lights.items[0].radius = radius * 8.0;
    graph.scene_lights.items[0].intensity = radius * radius;

    return .{
        .graph = graph,
        .mesh_allocs = mesh_allocs,
        .camera_node = camera_node,
        .near_plane = @max(radius * 0.01, 1e-3),
        .far_plane = distance + radius * 4.0,
        .allocator = allocator,
    };
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
