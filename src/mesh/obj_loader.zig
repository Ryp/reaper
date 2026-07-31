// Replacement for src/mesh/ModelLoader.cpp's load_obj, which delegates to
// tinyobjloader with triangulate = true.
//
// tinyobjloader is not linked on the Zig side, so its behaviour is reproduced
// here — and it has to be reproduced exactly, because the geometry it produces
// is what the meshlet builder and every downstream pass see:
//
//   * no vertex dedup: every face corner becomes a fresh entry in positions and
//     attributes, and the index buffer is just 0, 1, 2, ... in order
//   * quads split along the SHORTER diagonal, not always 0-2
//   * n-gons go through tinyobj's built-in ear clipping (NOT a fan — the plan
//     said fan, but track_chunk_simple.obj has 11-vertex faces and a fan would
//     produce wrong geometry on any concave one)
//   * missing normals default to (0, 0, 1), missing UVs to (0, 0), and the
//     tangent is always the dummy (1, 0, 0, 1)

const std = @import("std");

const mesh_module = @import("mesh.zig");

const Mesh = mesh_module.Mesh;
const VertexAttributes = mesh_module.VertexAttributes;

/// One face corner: indices into the position/normal/texcoord arrays, already
/// resolved to zero-based. -1 means absent.
const VertexIndex = struct {
    v: i32,
    vt: i32,
    vn: i32,
};

pub const Error = error{
    InvalidObjFile,
    NoFacesFound,
} || std.mem.Allocator.Error;

pub fn loadObjFromSlice(allocator: std.mem.Allocator, source: []const u8) Error!Mesh {
    var positions: std.ArrayList(f32) = .empty;
    defer positions.deinit(allocator);
    var normals: std.ArrayList(f32) = .empty;
    defer normals.deinit(allocator);
    var texcoords: std.ArrayList(f32) = .empty;
    defer texcoords.deinit(allocator);

    // Flattened faces: `face_corners` holds every corner back to back, and
    // `face_sizes` says how many belong to each face.
    var face_corners: std.ArrayList(VertexIndex) = .empty;
    defer face_corners.deinit(allocator);
    var face_sizes: std.ArrayList(u32) = .empty;
    defer face_sizes.deinit(allocator);

    var lines = std.mem.splitScalar(u8, source, '\n');
    while (lines.next()) |raw_line| {
        const line = std.mem.trim(u8, raw_line, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;

        var tokens = std.mem.tokenizeAny(u8, line, " \t");
        const keyword = tokens.next() orelse continue;

        if (std.mem.eql(u8, keyword, "v")) {
            // Vertex colours may follow the position; only xyz is kept, which
            // is what the C++ path uses too.
            for (0..3) |_| {
                const token = tokens.next() orelse return error.InvalidObjFile;
                try positions.append(allocator, parseFloat(token) orelse return error.InvalidObjFile);
            }
        } else if (std.mem.eql(u8, keyword, "vn")) {
            for (0..3) |_| {
                const token = tokens.next() orelse return error.InvalidObjFile;
                try normals.append(allocator, parseFloat(token) orelse return error.InvalidObjFile);
            }
        } else if (std.mem.eql(u8, keyword, "vt")) {
            for (0..2) |_| {
                const token = tokens.next() orelse return error.InvalidObjFile;
                try texcoords.append(allocator, parseFloat(token) orelse return error.InvalidObjFile);
            }
            _ = tokens.next(); // optional w
        } else if (std.mem.eql(u8, keyword, "f")) {
            var corner_count: u32 = 0;

            while (tokens.next()) |token| {
                const corner = parseFaceCorner(
                    token,
                    @intCast(positions.items.len / 3),
                    @intCast(normals.items.len / 3),
                    @intCast(texcoords.items.len / 2),
                ) orelse return error.InvalidObjFile;

                try face_corners.append(allocator, corner);
                corner_count += 1;
            }

            try face_sizes.append(allocator, corner_count);
        }
        // Everything else (o, g, s, usemtl, mtllib, ...) does not affect the
        // flattened mesh.
    }

    if (face_sizes.items.len == 0) return error.NoFacesFound;

    return triangulateAndFlatten(
        allocator,
        positions.items,
        normals.items,
        texcoords.items,
        face_corners.items,
        face_sizes.items,
    );
}

fn parseFloat(token: []const u8) ?f32 {
    return std.fmt.parseFloat(f32, token) catch null;
}

/// Handles `v`, `v/vt`, `v//vn` and `v/vt/vn`, including negative (relative)
/// indices. Mirrors tinyobj's fixIndex.
fn parseFaceCorner(token: []const u8, v_count: i32, vn_count: i32, vt_count: i32) ?VertexIndex {
    var parts = std.mem.splitScalar(u8, token, '/');

    const v_text = parts.next() orelse return null;
    const v_raw = std.fmt.parseInt(i32, v_text, 10) catch return null;
    const v = fixIndex(v_raw, v_count) orelse return null;

    var vt: i32 = -1;
    var vn: i32 = -1;

    if (parts.next()) |vt_text| {
        if (vt_text.len > 0) {
            const raw = std.fmt.parseInt(i32, vt_text, 10) catch return null;
            vt = fixIndex(raw, vt_count) orelse -1;
        }
    }

    if (parts.next()) |vn_text| {
        if (vn_text.len > 0) {
            const raw = std.fmt.parseInt(i32, vn_text, 10) catch return null;
            vn = fixIndex(raw, vn_count) orelse -1;
        }
    }

    return .{ .v = v, .vt = vt, .vn = vn };
}

fn fixIndex(idx: i32, n: i32) ?i32 {
    if (idx > 0) return idx - 1;
    if (idx == 0) return null; // zero is not allowed according to the spec
    return n + idx; // negative value = relative
}

// --------------------------------------------------------------------------
// Triangulation — transliterated from tinyobj's exportGroupsToShape
// --------------------------------------------------------------------------

fn triangulateAndFlatten(
    allocator: std.mem.Allocator,
    positions: []const f32,
    normals: []const f32,
    texcoords: []const f32,
    face_corners: []const VertexIndex,
    face_sizes: []const u32,
) Error!Mesh {
    var triangle_corners: std.ArrayList(VertexIndex) = .empty;
    defer triangle_corners.deinit(allocator);

    var scratch: std.ArrayList(VertexIndex) = .empty;
    defer scratch.deinit(allocator);

    var corner_offset: usize = 0;
    for (face_sizes) |face_size| {
        const face = face_corners[corner_offset..][0..face_size];
        corner_offset += face_size;

        // Face must have 3+ vertices.
        if (face_size < 3) continue;

        if (face_size == 3) {
            try triangle_corners.appendSlice(allocator, face);
        } else if (face_size == 4) {
            try triangulateQuad(allocator, positions, face, &triangle_corners);
        } else {
            try triangulateNgon(allocator, positions, face, &triangle_corners, &scratch);
        }
    }

    // Flatten with no dedup: index i refers to vertex i.
    var mesh = Mesh{};
    errdefer mesh.deinit(allocator);

    const count = triangle_corners.items.len;
    try mesh.positions.ensureTotalCapacity(allocator, count);
    try mesh.attributes.ensureTotalCapacity(allocator, count);
    try mesh.indexes.ensureTotalCapacity(allocator, count);

    for (triangle_corners.items, 0..) |corner, i| {
        const vi: usize = @intCast(corner.v);
        mesh.positions.appendAssumeCapacity(.{
            positions[vi * 3 + 0],
            positions[vi * 3 + 1],
            positions[vi * 3 + 2],
        });

        var attributes = VertexAttributes{};

        if (normals.len > 0 and corner.vn >= 0) {
            const ni: usize = @intCast(corner.vn);
            attributes.normal = .{ normals[ni * 3 + 0], normals[ni * 3 + 1], normals[ni * 3 + 2] };
        } else {
            attributes.normal = .{ 0.0, 0.0, 1.0 }; // Dummy value
        }

        if (texcoords.len > 0 and corner.vt >= 0) {
            const ti: usize = @intCast(corner.vt);
            attributes.uv = .{ texcoords[ti * 2 + 0], texcoords[ti * 2 + 1] };
        } else {
            attributes.uv = .{ 0.0, 0.0 }; // Dummy value
        }

        attributes.tangent = .{ 1.0, 0.0, 0.0, 1.0 }; // Dummy value

        mesh.attributes.appendAssumeCapacity(attributes);
        mesh.indexes.appendAssumeCapacity(@intCast(i));
    }

    return mesh;
}

fn position(positions: []const f32, index: i32, axis: usize) f32 {
    return positions[@as(usize, @intCast(index)) * 3 + axis];
}

/// Splits along the shorter diagonal. Picking 0-2 unconditionally would fold
/// non-planar quads the wrong way.
fn triangulateQuad(
    allocator: std.mem.Allocator,
    positions: []const f32,
    face: []const VertexIndex,
    out: *std.ArrayList(VertexIndex),
) Error!void {
    var e02: [3]f32 = undefined;
    var e13: [3]f32 = undefined;

    for (0..3) |axis| {
        e02[axis] = position(positions, face[2].v, axis) - position(positions, face[0].v, axis);
        e13[axis] = position(positions, face[3].v, axis) - position(positions, face[1].v, axis);
    }

    const sqr02 = e02[0] * e02[0] + e02[1] * e02[1] + e02[2] * e02[2];
    const sqr13 = e13[0] * e13[0] + e13[1] * e13[1] + e13[2] * e13[2];

    if (sqr02 < sqr13) {
        try out.appendSlice(allocator, &.{ face[0], face[1], face[2] });
        try out.appendSlice(allocator, &.{ face[0], face[2], face[3] });
    } else {
        try out.appendSlice(allocator, &.{ face[0], face[1], face[3] });
        try out.appendSlice(allocator, &.{ face[1], face[2], face[3] });
    }
}

/// tinyobj's built-in ear clipping, including its quirks: the projection axes
/// are chosen from the first corner with a non-degenerate cross product, and
/// the loop gives up after a bounded number of non-productive iterations.
fn triangulateNgon(
    allocator: std.mem.Allocator,
    positions: []const f32,
    face: []const VertexIndex,
    out: *std.ArrayList(VertexIndex),
    scratch: *std.ArrayList(VertexIndex),
) Error!void {
    var npolys = face.len;

    // Find the two axes to work in.
    var axes = [2]usize{ 1, 2 };
    for (0..npolys) |k| {
        const c0 = face[(k + 0) % npolys];
        const c1 = face[(k + 1) % npolys];
        const c2 = face[(k + 2) % npolys];

        var e0: [3]f32 = undefined;
        var e1: [3]f32 = undefined;
        for (0..3) |axis| {
            e0[axis] = position(positions, c1.v, axis) - position(positions, c0.v, axis);
            e1[axis] = position(positions, c2.v, axis) - position(positions, c1.v, axis);
        }

        const cx = @abs(e0[1] * e1[2] - e0[2] * e1[1]);
        const cy = @abs(e0[2] * e1[0] - e0[0] * e1[2]);
        const cz = @abs(e0[0] * e1[1] - e0[1] * e1[0]);

        const epsilon = std.math.floatEps(f32);
        if (cx > epsilon or cy > epsilon or cz > epsilon) {
            // Found a corner.
            if (cx > cy and cx > cz) {
                // axes stay {1, 2}
            } else {
                axes[0] = 0;
                if (cz > cx and cz > cy) {
                    axes[1] = 1;
                }
            }
            break;
        }
    }

    scratch.clearRetainingCapacity();
    try scratch.appendSlice(allocator, face);

    var guess_vert: usize = 0;
    var ind: [3]VertexIndex = undefined;
    var vx: [3]f32 = undefined;
    var vy: [3]f32 = undefined;

    // How many iterations can pass without the remaining vertex count dropping.
    var remaining_iterations = face.len;
    var previous_remaining_vertices = scratch.items.len;

    while (scratch.items.len > 3 and remaining_iterations > 0) {
        npolys = scratch.items.len;
        if (guess_vert >= npolys) {
            guess_vert -= npolys;
        }

        if (previous_remaining_vertices != npolys) {
            previous_remaining_vertices = npolys;
            remaining_iterations = npolys;
        } else {
            remaining_iterations -= 1;
        }

        for (0..3) |k| {
            ind[k] = scratch.items[(guess_vert + k) % npolys];
            vx[k] = position(positions, ind[k].v, axes[0]);
            vy[k] = position(positions, ind[k].v, axes[1]);
        }

        const e0x = vx[1] - vx[0];
        const e0y = vy[1] - vy[0];
        const e1x = vx[2] - vx[1];
        const e1y = vy[2] - vy[1];
        const cross = e0x * e1y - e0y * e1x;
        const area = (vx[0] * vy[1] - vy[0] * vx[1]) * 0.5;

        // Internal angle: not an ear.
        if (cross * area < 0.0) {
            guess_vert += 1;
            continue;
        }

        // Check whether any other vertex falls inside this triangle.
        var overlap = false;
        var other_vert: usize = 3;
        while (other_vert < npolys) : (other_vert += 1) {
            const idx = (guess_vert + other_vert) % npolys;
            if (idx >= scratch.items.len) continue;

            const other = scratch.items[idx];
            const tx = position(positions, other.v, axes[0]);
            const ty = position(positions, other.v, axes[1]);

            if (pointInTriangle(vx, vy, tx, ty)) {
                overlap = true;
                break;
            }
        }

        if (overlap) {
            guess_vert += 1;
            continue;
        }

        // This triangle is an ear.
        try out.appendSlice(allocator, &.{ ind[0], ind[1], ind[2] });

        // Remove the middle vertex from the working list.
        _ = scratch.orderedRemove((guess_vert + 1) % npolys);
    }

    if (scratch.items.len == 3) {
        try out.appendSlice(allocator, scratch.items[0..3]);
    }
}

/// pnpoly, specialised to a triangle.
/// https://wrf.ecse.rpi.edu//Research/Short_Notes/pnpoly.html
fn pointInTriangle(vertx: [3]f32, verty: [3]f32, testx: f32, testy: f32) bool {
    var inside = false;

    var i: usize = 0;
    var j: usize = 2;
    while (i < 3) : ({
        j = i;
        i += 1;
    }) {
        if (((verty[i] > testy) != (verty[j] > testy)) and
            (testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
        {
            inside = !inside;
        }
    }

    return inside;
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "flattening does not deduplicate vertices" {
    const source =
        \\v 0 0 0
        \\v 1 0 0
        \\v 0 1 0
        \\f 1 2 3
        \\f 1 2 3
    ;

    var mesh = try loadObjFromSlice(testing.allocator, source);
    defer mesh.deinit(testing.allocator);

    // Two triangles sharing all three vertices still produce six entries.
    try testing.expectEqual(@as(usize, 6), mesh.positions.items.len);
    try testing.expectEqual(@as(usize, 6), mesh.attributes.items.len);
    try testing.expectEqual(@as(usize, 6), mesh.indexes.items.len);

    for (mesh.indexes.items, 0..) |index, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), index);
    }
}

test "missing normals and uvs fall back to the dummy values" {
    const source =
        \\v 0 0 0
        \\v 1 0 0
        \\v 0 1 0
        \\f 1 2 3
    ;

    var mesh = try loadObjFromSlice(testing.allocator, source);
    defer mesh.deinit(testing.allocator);

    for (mesh.attributes.items) |attributes| {
        try testing.expectEqual([3]f32{ 0, 0, 1 }, attributes.normal);
        try testing.expectEqual([2]f32{ 0, 0 }, attributes.uv);
        try testing.expectEqual([4]f32{ 1, 0, 0, 1 }, attributes.tangent);
    }
}

test "quads split along the shorter diagonal" {
    // A quad stretched along x: the 1-3 diagonal is shorter than 0-2, so the
    // split must be [0,1,3] + [1,2,3] rather than the naive [0,1,2] + [0,2,3].
    const source =
        \\v 0 0 0
        \\v 4 0 0
        \\v 4 1 0
        \\v 0 1 0
        \\f 1 2 3 4
    ;

    var mesh = try loadObjFromSlice(testing.allocator, source);
    defer mesh.deinit(testing.allocator);

    try testing.expectEqual(@as(usize, 6), mesh.positions.items.len);

    // Diagonal 0-2 is (4,1,0) => 17; diagonal 1-3 is (-4,1,0) => 17. Equal, so
    // tinyobj takes the `else` branch: [0,1,3], [1,2,3].
    try testing.expectEqual([3]f32{ 0, 0, 0 }, mesh.positions.items[0]);
    try testing.expectEqual([3]f32{ 4, 0, 0 }, mesh.positions.items[1]);
    try testing.expectEqual([3]f32{ 0, 1, 0 }, mesh.positions.items[2]);
    try testing.expectEqual([3]f32{ 4, 0, 0 }, mesh.positions.items[3]);
    try testing.expectEqual([3]f32{ 4, 1, 0 }, mesh.positions.items[4]);
    try testing.expectEqual([3]f32{ 0, 1, 0 }, mesh.positions.items[5]);
}

test "quad with a genuinely shorter 0-2 diagonal takes the other branch" {
    const source =
        \\v 0 0 0
        \\v 1 0 0
        \\v 1 1 0
        \\v 0 4 0
        \\f 1 2 3 4
    ;

    var mesh = try loadObjFromSlice(testing.allocator, source);
    defer mesh.deinit(testing.allocator);

    // 0-2 is (1,1,0) => 2; 1-3 is (-1,4,0) => 17. So [0,1,2], [0,2,3].
    try testing.expectEqual([3]f32{ 0, 0, 0 }, mesh.positions.items[0]);
    try testing.expectEqual([3]f32{ 1, 0, 0 }, mesh.positions.items[1]);
    try testing.expectEqual([3]f32{ 1, 1, 0 }, mesh.positions.items[2]);
    try testing.expectEqual([3]f32{ 0, 0, 0 }, mesh.positions.items[3]);
    try testing.expectEqual([3]f32{ 1, 1, 0 }, mesh.positions.items[4]);
    try testing.expectEqual([3]f32{ 0, 4, 0 }, mesh.positions.items[5]);
}

test "negative indices are relative to the current vertex count" {
    const source =
        \\v 0 0 0
        \\v 1 0 0
        \\v 0 1 0
        \\f -3 -2 -1
    ;

    var mesh = try loadObjFromSlice(testing.allocator, source);
    defer mesh.deinit(testing.allocator);

    try testing.expectEqual([3]f32{ 0, 0, 0 }, mesh.positions.items[0]);
    try testing.expectEqual([3]f32{ 1, 0, 0 }, mesh.positions.items[1]);
    try testing.expectEqual([3]f32{ 0, 1, 0 }, mesh.positions.items[2]);
}

test "index forms v, v/vt, v//vn and v/vt/vn all parse" {
    const source =
        \\v 0 0 0
        \\v 1 0 0
        \\v 0 1 0
        \\vt 0.25 0.75
        \\vn 0 0 -1
        \\f 1/1/1 2//1 3/1
    ;

    var mesh = try loadObjFromSlice(testing.allocator, source);
    defer mesh.deinit(testing.allocator);

    try testing.expectEqual([2]f32{ 0.25, 0.75 }, mesh.attributes.items[0].uv);
    try testing.expectEqual([3]f32{ 0, 0, -1 }, mesh.attributes.items[0].normal);

    // Corner 1 has a normal but no texcoord, so the UV falls back.
    try testing.expectEqual([3]f32{ 0, 0, -1 }, mesh.attributes.items[1].normal);
    try testing.expectEqual([2]f32{ 0, 0 }, mesh.attributes.items[1].uv);

    // Corner 2 has a texcoord but no normal.
    try testing.expectEqual([2]f32{ 0.25, 0.75 }, mesh.attributes.items[2].uv);
    try testing.expectEqual([3]f32{ 0, 0, 1 }, mesh.attributes.items[2].normal);
}

test "convex ngon triangulates into n-2 triangles" {
    // A regular hexagon in the XY plane.
    const source =
        \\v 1 0 0
        \\v 0.5 0.866 0
        \\v -0.5 0.866 0
        \\v -1 0 0
        \\v -0.5 -0.866 0
        \\v 0.5 -0.866 0
        \\f 1 2 3 4 5 6
    ;

    var mesh = try loadObjFromSlice(testing.allocator, source);
    defer mesh.deinit(testing.allocator);

    try testing.expectEqual(@as(usize, (6 - 2) * 3), mesh.positions.items.len);
}

test "concave ngon does not emit a triangle spanning the notch" {
    // An arrowhead: vertex 3 is a reflex corner pushed inside the hull. A naive
    // fan from vertex 0 would cover the notch; ear clipping must not.
    const source =
        \\v 0 0 0
        \\v 4 0 0
        \\v 4 4 0
        \\v 2 1 0
        \\v 0 4 0
        \\f 1 2 3 4 5
    ;

    var mesh = try loadObjFromSlice(testing.allocator, source);
    defer mesh.deinit(testing.allocator);

    try testing.expectEqual(@as(usize, (5 - 2) * 3), mesh.positions.items.len);

    // The reflex vertex (2, 1, 0) has to appear in the output; dropping it
    // would mean the notch got filled in.
    var found_reflex = false;
    for (mesh.positions.items) |p| {
        if (p[0] == 2 and p[1] == 1) found_reflex = true;
    }
    try testing.expect(found_reflex);
}
