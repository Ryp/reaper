// The meshlet build half of src/renderer/vulkan/MeshCache.cpp.
//
// Kept out of the Vulkan layer because it is pure mesh processing: it needs
// meshoptimizer and the shared Meshlet layout, but no device. That makes it
// testable in the GPU-free test artifact, which matters — the clusterizer's
// output is what every culling and drawing pass downstream depends on.

const std = @import("std");

const hlsl_meshlet = @import("../renderer/hlsl/meshlet/meshlet.zig");
const mesh_module = @import("mesh.zig");
const meshopt = @import("meshoptimizer.zig").c;

const Mesh = mesh_module.Mesh;
const Meshlet = hlsl_meshlet.Meshlet;
const VertexAttributes = mesh_module.VertexAttributes;

/// Clusterizer parameters. MeshletMaxTriangleCount comes from the shared HLSL
/// header, so CPU and GPU agree on the cluster size.
pub const max_vertices: usize = hlsl_meshlet.MeshletMaxTriangleCount * 3;
pub const max_triangles: usize = hlsl_meshlet.MeshletMaxTriangleCount;
pub const cone_weight: f32 = 0.5; // FIXME fold in a constant

comptime {
    // NOTE: u8 indices
    std.debug.assert(max_vertices < 256);
}

/// The result of clusterizing one mesh: vertex streams reordered into meshlet
/// order, and one compacted index buffer.
pub const MeshletMesh = struct {
    indexes: []u32,
    positions: [][3]f32,
    attributes: []VertexAttributes,
    meshlets: []Meshlet,

    pub fn deinit(self: *MeshletMesh, allocator: std.mem.Allocator) void {
        allocator.free(self.meshlets);
        allocator.free(self.attributes);
        allocator.free(self.positions);
        allocator.free(self.indexes);
    }
};

/// Runs meshoptimizer's clusterizer and rebuilds the vertex streams in meshlet
/// order. Split out from the upload so it can be tested without a device.
pub fn buildMeshlets(allocator: std.mem.Allocator, mesh: Mesh) !MeshletMesh {
    std.debug.assert(mesh.attributes.items.len == mesh.positions.items.len);

    const max_meshlets = meshopt.meshopt_buildMeshletsBound(
        mesh.indexes.items.len,
        max_vertices,
        max_triangles,
    );

    const raw_meshlets = try allocator.alloc(meshopt.meshopt_Meshlet, max_meshlets);
    defer allocator.free(raw_meshlets);

    const meshlet_vertices = try allocator.alloc(u32, max_meshlets * max_vertices);
    defer allocator.free(meshlet_vertices);

    const meshlet_indices = try allocator.alloc(u8, max_meshlets * max_triangles * 3);
    defer allocator.free(meshlet_indices);

    const meshlet_count = meshopt.meshopt_buildMeshlets(
        raw_meshlets.ptr,
        meshlet_vertices.ptr,
        meshlet_indices.ptr,
        mesh.indexes.items.ptr,
        mesh.indexes.items.len,
        @ptrCast(mesh.positions.items.ptr),
        mesh.positions.items.len,
        @sizeOf([3]f32),
        max_vertices,
        max_triangles,
        cone_weight,
    );

    std.debug.assert(meshlet_count > 0);

    // Trim the over-allocated arrays down to what the clusterizer actually used.
    const last = raw_meshlets[meshlet_count - 1];
    const used_meshlet_vertices = meshlet_vertices[0 .. last.vertex_offset + last.vertex_count];
    const used_meshlet_indices = meshlet_indices[0 .. last.triangle_offset + last.triangle_count * 3];
    const used_meshlets = raw_meshlets[0..meshlet_count];

    const meshlet_vertex_count: u32 = @intCast(used_meshlet_vertices.len);
    const total_mesh_index_count: u32 = @intCast(used_meshlet_indices.len);

    var optimized_index_buffer = try allocator.alloc(u32, total_mesh_index_count);
    errdefer allocator.free(optimized_index_buffer);

    const optimized_position_buffer = try allocator.alloc([3]f32, meshlet_vertex_count);
    errdefer allocator.free(optimized_position_buffer);

    const optimized_attributes_buffer = try allocator.alloc(VertexAttributes, meshlet_vertex_count);
    errdefer allocator.free(optimized_attributes_buffer);

    const optimized_meshlets = try allocator.alloc(Meshlet, meshlet_count);
    errdefer allocator.free(optimized_meshlets);

    for (used_meshlet_vertices, 0..) |index, i| {
        optimized_position_buffer[i] = mesh.positions.items[index];
        optimized_attributes_buffer[i] = mesh.attributes.items[index];
    }

    var index_output_offset: u32 = 0;

    // Index buffer compaction happens in the same pass.
    for (used_meshlets, 0..) |meshlet, meshlet_index| {
        const bounds = meshopt.meshopt_computeMeshletBounds(
            &meshlet_vertices[meshlet.vertex_offset],
            &meshlet_indices[meshlet.triangle_offset],
            meshlet.triangle_count,
            @ptrCast(mesh.positions.items.ptr),
            mesh.positions.items.len,
            @sizeOf([3]f32),
        );

        const meshlet_index_count = meshlet.triangle_count * 3;

        optimized_meshlets[meshlet_index] = .{
            .vertex_offset = meshlet.vertex_offset,
            .vertex_count = meshlet.vertex_count,
            .index_offset = index_output_offset,
            .index_count = meshlet_index_count,
            .center_ms = .{ .x = bounds.center[0], .y = bounds.center[1], .z = bounds.center[2] },
            .radius = bounds.radius,
            .cone_axis_ms = .{ .x = bounds.cone_axis[0], .y = bounds.cone_axis[1], .z = bounds.cone_axis[2] },
            .cone_cutoff = bounds.cone_cutoff,
            .cone_apex_ms = .{ .x = bounds.cone_apex[0], .y = bounds.cone_apex[1], .z = bounds.cone_apex[2] },
        };

        // The meshlet's triangle indices are u8 relative to its vertex range;
        // widen them into the shared u32 index buffer.
        for (0..meshlet_index_count) |index| {
            optimized_index_buffer[index_output_offset + index] =
                meshlet_indices[meshlet.triangle_offset + index];
        }

        index_output_offset += meshlet_index_count;
    }

    // Trim excess
    optimized_index_buffer = try allocator.realloc(optimized_index_buffer, index_output_offset);

    return .{
        .indexes = optimized_index_buffer,
        .positions = optimized_position_buffer,
        .attributes = optimized_attributes_buffer,
        .meshlets = optimized_meshlets,
    };
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;
const obj_loader = @import("obj_loader.zig");

fn buildFromObj(allocator: std.mem.Allocator, source: []const u8) !MeshletMesh {
    var mesh = try obj_loader.loadObjFromSlice(allocator, source);
    defer mesh.deinit(allocator);

    return buildMeshlets(allocator, mesh);
}

test "a single triangle produces one meshlet" {
    const source =
        \\v 0 0 0
        \\v 1 0 0
        \\v 0 1 0
        \\f 1 2 3
    ;

    var meshlet_mesh = try buildFromObj(testing.allocator, source);
    defer meshlet_mesh.deinit(testing.allocator);

    try testing.expectEqual(@as(usize, 1), meshlet_mesh.meshlets.len);
    try testing.expectEqual(@as(u32, 3), meshlet_mesh.meshlets[0].index_count);
    try testing.expectEqual(@as(u32, 0), meshlet_mesh.meshlets[0].index_offset);
    try testing.expectEqual(@as(usize, 3), meshlet_mesh.indexes.len);
    try testing.expectEqual(@as(usize, 3), meshlet_mesh.positions.len);
    try testing.expectEqual(meshlet_mesh.positions.len, meshlet_mesh.attributes.len);
}

test "meshlet index runs are compacted back to back" {
    // Enough triangles to spill into several meshlets.
    var source: std.ArrayList(u8) = .empty;
    defer source.deinit(testing.allocator);

    const triangle_count = 500;
    for (0..triangle_count) |i| {
        const x: f32 = @floatFromInt(i);
        try source.print(testing.allocator,
            \\v {d} 0 0
            \\v {d} 1 0
            \\v {d} 0 1
            \\
        , .{ x, x, x });
    }
    for (0..triangle_count) |i| {
        const base = i * 3 + 1;
        try source.print(testing.allocator, "f {d} {d} {d}\n", .{ base, base + 1, base + 2 });
    }

    var meshlet_mesh = try buildFromObj(testing.allocator, source.items);
    defer meshlet_mesh.deinit(testing.allocator);

    try testing.expect(meshlet_mesh.meshlets.len > 1);

    // Every meshlet's index range must start exactly where the previous one
    // ended: that is what index buffer compaction means, and the culling passes
    // read the buffer on that assumption.
    var expected_offset: u32 = 0;
    for (meshlet_mesh.meshlets) |meshlet| {
        try testing.expectEqual(expected_offset, meshlet.index_offset);
        try testing.expectEqual(@as(u32, 0), meshlet.index_count % 3);
        try testing.expect(meshlet.index_count <= max_triangles * 3);
        expected_offset += meshlet.index_count;
    }

    try testing.expectEqual(@as(usize, expected_offset), meshlet_mesh.indexes.len);
}

test "meshlet indices stay inside their own vertex range" {
    var source: std.ArrayList(u8) = .empty;
    defer source.deinit(testing.allocator);

    const triangle_count = 300;
    for (0..triangle_count) |i| {
        const x: f32 = @floatFromInt(i % 17);
        const y: f32 = @floatFromInt(i / 17);
        try source.print(testing.allocator,
            \\v {d} {d} 0
            \\v {d} {d} 0
            \\v {d} {d} 1
            \\
        , .{ x, y, x + 1, y, x, y + 1 });
    }
    for (0..triangle_count) |i| {
        const base = i * 3 + 1;
        try source.print(testing.allocator, "f {d} {d} {d}\n", .{ base, base + 1, base + 2 });
    }

    var meshlet_mesh = try buildFromObj(testing.allocator, source.items);
    defer meshlet_mesh.deinit(testing.allocator);

    // Indices are meshlet-local: each one must be < that meshlet's vertex
    // count, and vertex_offset + vertex_count must stay inside the reordered
    // vertex buffer.
    for (meshlet_mesh.meshlets) |meshlet| {
        try testing.expect(meshlet.vertex_offset + meshlet.vertex_count <= meshlet_mesh.positions.len);
        try testing.expect(meshlet.vertex_count <= max_vertices);

        const indices = meshlet_mesh.indexes[meshlet.index_offset..][0..meshlet.index_count];
        for (indices) |index| {
            try testing.expect(index < meshlet.vertex_count);
        }
    }
}

test "meshlet bounds enclose their own vertices" {
    var source: std.ArrayList(u8) = .empty;
    defer source.deinit(testing.allocator);

    const triangle_count = 200;
    for (0..triangle_count) |i| {
        const x: f32 = @floatFromInt(i % 13);
        const z: f32 = @floatFromInt(i / 13);
        try source.print(testing.allocator,
            \\v {d} 0 {d}
            \\v {d} 0 {d}
            \\v {d} 1 {d}
            \\
        , .{ x, z, x + 1, z, x, z });
    }
    for (0..triangle_count) |i| {
        const base = i * 3 + 1;
        try source.print(testing.allocator, "f {d} {d} {d}\n", .{ base, base + 1, base + 2 });
    }

    var meshlet_mesh = try buildFromObj(testing.allocator, source.items);
    defer meshlet_mesh.deinit(testing.allocator);

    // The culling passes reject meshlets on these spheres, so a sphere that
    // does not actually contain its geometry would drop visible triangles.
    for (meshlet_mesh.meshlets) |meshlet| {
        const vertices = meshlet_mesh.positions[meshlet.vertex_offset..][0..meshlet.vertex_count];

        for (vertices) |p| {
            const dx = p[0] - meshlet.center_ms.x;
            const dy = p[1] - meshlet.center_ms.y;
            const dz = p[2] - meshlet.center_ms.z;
            const distance = @sqrt(dx * dx + dy * dy + dz * dz);

            try testing.expect(distance <= meshlet.radius + 1e-3);
        }
    }
}
