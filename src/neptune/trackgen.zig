// Port of src/neptune/trackgen/Track.{h,cpp}
//
// A track is a chain of spherical chunks: each node picks a radius and a random
// deviation from the previous node's exit frame, and is rejected if its bounding
// sphere overlaps an earlier one. A straight chunk mesh is then skinned along
// each node's arc by two bones.
//
// DEVIATION: the C++ seeds std::mt19937 from std::random_device, so its tracks
// are not reproducible and cannot be matched here anyway. This uses a seeded
// std.Random.DefaultPrng, which makes the Zig track reproducible run to run.
// Everything downstream of the random angles is a faithful port.

const std = @import("std");

const linalg = @import("../math/linalg.zig");
const mesh_module = @import("../mesh/mesh.zig");

const Mat4x3 = linalg.Mat4x3;
const Vec3 = linalg.Vec3;

/// Port of src/neptune/Constants.h.
pub const meter_in_game_units: f32 = 0.01;

const half_pi: f32 = 1.57079632679;

const theta_max: f32 = 0.8 * half_pi;
const phi_max: f32 = 1.0 * std.math.pi;
const roll_max: f32 = 0.25 * std.math.pi;
const width_min: f32 = 20.0 * meter_in_game_units;
const width_max: f32 = 50.0 * meter_in_game_units;

const min_length: u32 = 3;
const max_length: u32 = 1000;
const max_try_count: u32 = 10000;

pub const bone_count_per_chunk: u32 = 2;

const unit_x_axis = Vec3{ 1, 0, 0 };
const unit_z_axis = Vec3{ 0, 0, 1 };

pub const default_radius_min_meter: f32 = 100.0;
pub const default_radius_max_meter: f32 = 200.0;

pub const GenerationInfo = struct {
    chunk_count: u32 = 10,
    radius_min_meter: f32 = default_radius_min_meter,
    radius_max_meter: f32 = default_radius_max_meter,
    chaos: f32 = 0.2,
};

pub const TrackSkeletonNode = struct {
    /// Transform of the input frame placed on the tangent of the bounding sphere.
    in_transform_ms_to_ws: Mat4x3,
    end_transform: Mat4x3,

    phi_angle: f32,
    theta_angle: f32,
    roll_angle: f32,

    radius: f32,
    in_width: f32,
    out_width: f32,

    // Extra
    center_ws: Vec3,
    out_transform_ms_to_ws: Mat4x3,

    // Matrix inverses
    in_transform_ws_to_ms: Mat4x3,
    out_transform_ws_to_ms: Mat4x3,
};

pub const TrackSkinning = struct {
    bind_pose_inv_transforms: [bone_count_per_chunk]Mat4x3,
    pose_transforms: [bone_count_per_chunk]Mat4x3,
};

// --------------------------------------------------------------------------
// Skeleton
// --------------------------------------------------------------------------

fn uniform(random: std.Random, min: f32, max: f32) f32 {
    return min + random.float(f32) * (max - min);
}

const ChunkEnd = struct {
    transform: Mat4x3,
    phi_angle: f32,
    theta_angle: f32,
    roll_angle: f32,
};

fn generateChunkEnd(gen_info: GenerationInfo, random: std.Random) ChunkEnd {
    const theta = uniform(random, 0.0, theta_max * gen_info.chaos);
    const phi = uniform(random, -phi_max, phi_max);
    const roll = uniform(random, -roll_max * gen_info.chaos, roll_max * gen_info.chaos);

    const deviation = linalg.mulQuat(
        linalg.angleAxis(phi, unit_x_axis),
        linalg.angleAxis(theta, unit_z_axis),
    );
    const roll_fixup = linalg.angleAxis(-phi + roll, linalg.rotateVec3(deviation, unit_x_axis));

    return .{
        .transform = linalg.mat4x3FromMat4(linalg.quatToMat4(linalg.mulQuat(roll_fixup, deviation))),
        .phi_angle = phi,
        .theta_angle = theta,
        .roll_angle = roll,
    };
}

/// NOTE: the last node is deliberately excluded — the C++ loop condition is
/// `(i + 1) < nodes.size()`, so a node never collides with its own predecessor.
fn findSelfCollision(nodes: []const TrackSkeletonNode, current_node: TrackSkeletonNode) ?u32 {
    if (nodes.len == 0) return null;

    for (nodes[0 .. nodes.len - 1], 0..) |node, i| {
        const delta = current_node.center_ws - node.center_ws;
        const distance_sq = linalg.dot(delta, delta);
        const min_radius = current_node.radius + node.radius;

        if (distance_sq < min_radius * min_radius) return @intCast(i);
    }

    return null;
}

fn generateNode(
    gen_info: GenerationInfo,
    previous_nodes: []const TrackSkeletonNode,
    random: std.Random,
) TrackSkeletonNode {
    var node: TrackSkeletonNode = undefined;

    node.radius = uniform(
        random,
        gen_info.radius_min_meter * meter_in_game_units,
        gen_info.radius_max_meter * meter_in_game_units,
    );

    const forward_offset_ms = unit_x_axis * linalg.splat(Vec3, node.radius);

    if (previous_nodes.len == 0) {
        node.in_transform_ms_to_ws = Mat4x3.identity;
        node.in_width = uniform(random, width_min, width_max);
        node.center_ws = forward_offset_ms;
    } else {
        const previous_node = previous_nodes[previous_nodes.len - 1];

        node.in_transform_ms_to_ws = previous_node.out_transform_ms_to_ws;
        node.in_width = previous_node.out_width;
        node.center_ws = linalg.mulMat4x3Vec4(node.in_transform_ms_to_ws, linalg.vec4FromVec3(forward_offset_ms, 1.0));
    }

    const chunk_end = generateChunkEnd(gen_info, random);
    node.end_transform = chunk_end.transform;
    node.phi_angle = chunk_end.phi_angle;
    node.theta_angle = chunk_end.theta_angle;
    node.roll_angle = chunk_end.roll_angle;

    const translation_a = linalg.translate(forward_offset_ms);
    const translation_b = linalg.translate(
        linalg.mulMat4x3Vec4(node.end_transform, linalg.vec4FromVec3(forward_offset_ms, 0.0)),
    );

    node.out_transform_ms_to_ws = linalg.mat4x3FromMat4(linalg.mulMat4(
        linalg.mulMat4(
            linalg.mulMat4(linalg.mat4FromMat4x3(node.in_transform_ms_to_ws), translation_a),
            translation_b,
        ),
        linalg.mat4FromMat4x3(node.end_transform),
    ));
    node.out_width = uniform(random, width_min, width_max);

    node.in_transform_ws_to_ms = linalg.mat4x3FromMat4(
        linalg.inverseMat4(linalg.mat4FromMat4x3(node.in_transform_ms_to_ws)),
    );
    node.out_transform_ws_to_ms = linalg.mat4x3FromMat4(
        linalg.inverseMat4(linalg.mat4FromMat4x3(node.out_transform_ms_to_ws)),
    );

    return node;
}

pub fn generateTrackSkeleton(
    gen_info: GenerationInfo,
    skeleton_nodes: []TrackSkeletonNode,
    random: std.Random,
) !void {
    std.debug.assert(gen_info.chunk_count >= min_length);
    std.debug.assert(gen_info.chunk_count <= max_length);
    std.debug.assert(gen_info.chunk_count == skeleton_nodes.len);

    var try_count: u32 = 0;
    var current_node_index: u32 = 0;

    while (current_node_index < gen_info.chunk_count and try_count < max_try_count) {
        const generated_nodes = skeleton_nodes[0..current_node_index];
        const new_node = generateNode(gen_info, generated_nodes, random);

        // On a collision, rewind to just past the node that was hit and retry
        // the rest of the chain.
        if (findSelfCollision(generated_nodes, new_node)) |collider_index| {
            current_node_index = collider_index + 1;
        } else {
            skeleton_nodes[current_node_index] = new_node;
            current_node_index += 1;
        }

        try_count += 1;
    }

    // "something is majorly FUBAR"
    if (try_count >= max_try_count) return error.TrackGenerationFailed;
}

// --------------------------------------------------------------------------
// Skinning
// --------------------------------------------------------------------------

fn generateTrackSkinningForChunk(node: TrackSkeletonNode) TrackSkinning {
    var skinning: TrackSkinning = undefined;

    // The inverse of a translation is the translation by the opposite vector.
    for (&skinning.bind_pose_inv_transforms, 0..) |*transform, bone_index| {
        const t = @as(f32, @floatFromInt(bone_index)) / @as(f32, @floatFromInt(bone_count_per_chunk - 1));
        const offset = unit_x_axis * linalg.splat(Vec3, t * node.radius * 2.0);

        transform.* = linalg.mat4x3FromMat4(linalg.translate(-offset));
    }

    // NOTE: local transform origin is the 'in' node, not the sphere center.
    var bone_root_positions_ms: [bone_count_per_chunk]Vec3 = undefined;

    bone_root_positions_ms[0] = @splat(0.0);
    // FIXME We need to fix positions to be placed on the arc of the trajectory.
    bone_root_positions_ms[1] = unit_x_axis * linalg.splat(Vec3, node.radius) +
        linalg.mulMat4x3Vec4(
            node.end_transform,
            linalg.vec4FromVec3(unit_x_axis * linalg.splat(Vec3, node.radius), 0.0),
        ); // FIXME

    for (&skinning.pose_transforms, 0..) |*pose_transform, bone_index| {
        const t = @as(f32, @floatFromInt(bone_index)) / @as(f32, @floatFromInt(bone_count_per_chunk - 1));
        const theta_angle = t * node.theta_angle;
        const roll_angle = t * node.roll_angle;

        const deviation = linalg.mulQuat(
            linalg.angleAxis(node.phi_angle, unit_x_axis),
            linalg.angleAxis(theta_angle, unit_z_axis),
        );
        const roll_fixup = linalg.angleAxis(
            -node.phi_angle + roll_angle,
            linalg.rotateVec3(deviation, unit_x_axis),
        );

        const transform = linalg.translate(bone_root_positions_ms[bone_index]);

        pose_transform.* = linalg.mat4x3FromMat4(linalg.mulMat4(
            transform,
            linalg.quatToMat4(linalg.mulQuat(roll_fixup, deviation)),
        ));
    }

    return skinning;
}

pub fn generateTrackSkinning(
    skeleton_nodes: []const TrackSkeletonNode,
    skinning: []TrackSkinning,
) void {
    std.debug.assert(skeleton_nodes.len == skinning.len);

    for (skeleton_nodes, skinning) |node, *out| {
        out.* = generateTrackSkinningForChunk(node);
    }
}

/// Linear falloff between the two bones — the weights always sum to 1.
fn computeTrackBoneWeights(t: f32) [bone_count_per_chunk]f32 {
    var weights: [bone_count_per_chunk]f32 = undefined;
    var debug_sum: f32 = 0.0;

    for (&weights, 0..) |*weight, bone_index| {
        const bones: f32 = @floatFromInt(bone_count_per_chunk - 1);
        const index: f32 = @floatFromInt(bone_index);

        weight.* = @max(0.0, 1.0 - @abs(t * bones - index));
        debug_sum += weight.*;
    }

    std.debug.assert(@abs(debug_sum - 1.0) < 1e-4);

    return weights;
}

/// Stretches the straight chunk mesh to the node's arc length and bends it
/// along the node's two bones, in place.
pub fn skinTrackChunkMesh(
    node: TrackSkeletonNode,
    track_skinning: TrackSkinning,
    vertices: [][3]f32,
    mesh_length: f32,
) void {
    var bone_transforms: [bone_count_per_chunk]Mat4x3 = undefined;

    for (&bone_transforms, track_skinning.pose_transforms, track_skinning.bind_pose_inv_transforms) |*out, pose, bind_inv| {
        out.* = linalg.mat4x3FromMat4(linalg.mulMat4(
            linalg.mat4FromMat4x3(pose),
            linalg.mat4FromMat4x3(bind_inv),
        ));
    }

    std.debug.assert(vertices.len > 0);

    const chunk_length = node.radius * 2.0;
    const scale_x = chunk_length / mesh_length;

    for (vertices) |*vertex_out| {
        const vertex = Vec3{ vertex_out[0], vertex_out[1], vertex_out[2] } * Vec3{ scale_x, 1.0, 1.0 };
        const bone_weights = computeTrackBoneWeights(vertex[0] / chunk_length);

        var skinned_position: Vec3 = @splat(0.0);
        var weight_sum: f32 = 0.0;

        for (bone_transforms, bone_weights) |transform, weight| {
            const transformed = linalg.mulMat4x3Vec4(transform, linalg.vec4FromVec3(vertex, 1.0));

            skinned_position += transformed * linalg.splat(Vec3, weight);
            weight_sum += weight;
        }

        if (@abs(weight_sum) > 0.0) {
            skinned_position /= linalg.splat(Vec3, weight_sum);
        }

        vertex_out.* = .{ skinned_position[0], skinned_position[1], skinned_position[2] };
    }
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "a generated skeleton has no self-intersecting chunks" {
    const gen_info = GenerationInfo{
        .chunk_count = 100,
        .radius_min_meter = 300.0,
        .radius_max_meter = 600.0,
        .chaos = 0.4,
    };

    var prng = std.Random.DefaultPrng.init(0x5EED);
    var nodes: [100]TrackSkeletonNode = undefined;

    try generateTrackSkeleton(gen_info, &nodes, prng.random());

    // Rejection is the whole point of the generator, so the invariant it
    // enforces has to hold on the output — for every pair, not just the ones
    // the incremental check happened to look at.
    for (nodes, 0..) |a, i| {
        try testing.expect(a.radius >= gen_info.radius_min_meter * meter_in_game_units);
        try testing.expect(a.radius <= gen_info.radius_max_meter * meter_in_game_units);

        for (nodes[i + 1 ..], i + 1..) |b, j| {
            // Consecutive chunks share a frame and are meant to touch.
            if (j == i + 1) continue;

            const delta = a.center_ws - b.center_ws;
            const min_radius = a.radius + b.radius;

            try testing.expect(linalg.dot(delta, delta) >= min_radius * min_radius);
        }
    }
}

test "chunks chain end to end" {
    var prng = std.Random.DefaultPrng.init(1);
    var nodes: [8]TrackSkeletonNode = undefined;

    try generateTrackSkeleton(.{ .chunk_count = 8 }, &nodes, prng.random());

    // Each node enters where the previous one left, and its inverse is a real
    // inverse.
    for (nodes[1..], nodes[0 .. nodes.len - 1]) |node, previous| {
        inline for (0..4) |col| {
            inline for (0..3) |row| {
                try testing.expectApproxEqAbs(
                    previous.out_transform_ms_to_ws.c[col][row],
                    node.in_transform_ms_to_ws.c[col][row],
                    1e-6,
                );
            }
        }
    }

    for (nodes) |node| {
        const round_trip = linalg.mulMat4(
            linalg.mat4FromMat4x3(node.out_transform_ms_to_ws),
            linalg.mat4FromMat4x3(node.out_transform_ws_to_ms),
        );

        inline for (0..4) |col| {
            inline for (0..4) |row| {
                const expected: f32 = if (col == row) 1.0 else 0.0;
                try testing.expectApproxEqAbs(expected, round_trip.c[col][row], 1e-4);
            }
        }
    }
}

test "bone weights always sum to one and fall off linearly" {
    try testing.expectEqual([_]f32{ 1.0, 0.0 }, computeTrackBoneWeights(0.0));
    try testing.expectEqual([_]f32{ 0.0, 1.0 }, computeTrackBoneWeights(1.0));
    try testing.expectEqual([_]f32{ 0.5, 0.5 }, computeTrackBoneWeights(0.5));

    for (0..21) |i| {
        const t = @as(f32, @floatFromInt(i)) / 20.0;
        const weights = computeTrackBoneWeights(t);

        try testing.expectApproxEqAbs(@as(f32, 1.0), weights[0] + weights[1], 1e-6);
    }
}

test "skinning stretches a straight chunk to the node's length" {
    var prng = std.Random.DefaultPrng.init(7);
    var nodes: [3]TrackSkeletonNode = undefined;

    try generateTrackSkeleton(.{ .chunk_count = 3, .chaos = 0.0 }, &nodes, prng.random());

    var skinning: [3]TrackSkinning = undefined;
    generateTrackSkinning(&nodes, &skinning);

    // With chaos 0 the chunk stays straight, so skinning reduces to a scale
    // along X — which makes the expected output exact.
    const mesh_length: f32 = 10.0;
    var vertices = [_][3]f32{
        .{ 0.0, 1.0, 2.0 },
        .{ mesh_length * 0.5, -1.0, 0.5 },
        .{ mesh_length, 0.0, -3.0 },
    };

    skinTrackChunkMesh(nodes[0], skinning[0], &vertices, mesh_length);

    const chunk_length = nodes[0].radius * 2.0;

    try testing.expectApproxEqAbs(@as(f32, 0.0), vertices[0][0], 1e-5);
    try testing.expectApproxEqAbs(chunk_length * 0.5, vertices[1][0], 1e-5);
    try testing.expectApproxEqAbs(chunk_length, vertices[2][0], 1e-5);

    // A straight chunk leaves the cross-section alone.
    try testing.expectApproxEqAbs(@as(f32, 1.0), vertices[0][1], 1e-5);
    try testing.expectApproxEqAbs(@as(f32, 2.0), vertices[0][2], 1e-5);
    try testing.expectApproxEqAbs(@as(f32, -3.0), vertices[2][2], 1e-5);
}

test "skinning matches the C++ for a fixed node" {
    // The generator's RNG cannot be matched (see the file comment), but
    // everything downstream of the random angles can be: this node's angles are
    // hardcoded, and the expectations below come from a probe compiled against
    // src/neptune/trackgen/Track.cpp with the vendored glm.
    var node = std.mem.zeroes(TrackSkeletonNode);
    node.radius = 4.25;
    node.phi_angle = 0.7;
    node.theta_angle = 0.35;
    node.roll_angle = -0.2;

    {
        const deviation = linalg.mulQuat(
            linalg.angleAxis(node.phi_angle, unit_x_axis),
            linalg.angleAxis(node.theta_angle, unit_z_axis),
        );
        const roll_fixup = linalg.angleAxis(
            -node.phi_angle + node.roll_angle,
            linalg.rotateVec3(deviation, unit_x_axis),
        );

        node.end_transform = linalg.mat4x3FromMat4(linalg.quatToMat4(linalg.mulQuat(roll_fixup, deviation)));
    }

    try expectMat4x3(.{
        0.939372718,  0.262262732, 0.220900849,
        -0.213148743, 0.951242328, -0.222947598,
        -0.26860109,  0.162346154, 0.94947207,
        0,            0,           0,
    }, node.end_transform);

    var nodes = [_]TrackSkeletonNode{node};
    var skinning: [1]TrackSkinning = undefined;
    generateTrackSkinning(&nodes, &skinning);

    try expectMat4x3(.{ 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 }, skinning[0].bind_pose_inv_transforms[0]);
    try expectMat4x3(.{ 1, 0, 0, 0, 1, 0, 0, 0, 1, -8.5, 0, 0 }, skinning[0].bind_pose_inv_transforms[1]);

    try expectMat4x3(.{ 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 }, skinning[0].pose_transforms[0]);
    try expectMat4x3(.{
        0.939372718,  0.262262732, 0.220900849,
        -0.213148743, 0.951242328, -0.222947598,
        -0.26860109,  0.162346154, 0.94947207,
        8.24233437,   1.11461663,  0.938828588,
    }, skinning[0].pose_transforms[1]);

    var vertices = [_][3]f32{
        .{ 0.0, 0.0, 0.0 },
        .{ 2.5, 1.0, -1.0 },
        .{ 5.0, -0.5, 0.25 },
        .{ 7.5, 0.0, 2.0 },
        .{ 10.0, 1.5, -2.5 },
    };

    skinTrackChunkMesh(node, skinning[0], &vertices, 10.0);

    const expected = [_][3]f32{
        .{ 0, 0, 0 },
        .{ 2.17107153, 0.807896972, -1.16045856 },
        .{ 4.26971245, -0.467517316, 0.299420893 },
        .{ 5.8754735, 0.661500514, 2.27626896 },
        .{ 8.5941143, 2.13561463, -1.76927292 },
    };

    for (vertices, expected) |actual, want| {
        for (actual, want) |a, w| {
            try testing.expectApproxEqAbs(w, a, 1e-6);
        }
    }
}

/// Column-major, three rows per column — the order the C++ probe prints.
fn expectMat4x3(expected: [12]f32, actual: Mat4x3) !void {
    inline for (0..4) |col| {
        inline for (0..3) |row| {
            testing.expectApproxEqAbs(expected[col * 3 + row], actual.c[col][row], 1e-6) catch |err| {
                std.debug.print("mismatch at c[{}][{}]\n", .{ col, row });
                return err;
            };
        }
    }
}

test "a bent chunk actually bends" {
    var prng = std.Random.DefaultPrng.init(11);
    var nodes: [3]TrackSkeletonNode = undefined;

    try generateTrackSkeleton(.{ .chunk_count = 3, .chaos = 1.0 }, &nodes, prng.random());

    var skinning: [3]TrackSkinning = undefined;
    generateTrackSkinning(&nodes, &skinning);

    const mesh_length: f32 = 10.0;
    var vertices = [_][3]f32{.{ mesh_length, 0.0, 0.0 }};

    skinTrackChunkMesh(nodes[0], skinning[0], &vertices, mesh_length);

    // The far end lands on the node's exit frame rather than straight ahead.
    const straight_ahead = nodes[0].radius * 2.0;
    const end = Vec3{ vertices[0][0], vertices[0][1], vertices[0][2] };

    try testing.expect(linalg.length(end - Vec3{ straight_ahead, 0, 0 }) > 1e-3);
    // ...but the arc length is preserved to within the chord shortening.
    try testing.expect(linalg.length(end) <= straight_ahead + 1e-3);
}
