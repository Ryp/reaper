// Port of src/renderer/Camera.{h,cpp}
//
// Two quirks are ported deliberately and must not be "fixed":
//
//   1. build_renderer_perspective_projection passes the *horizontal* half-FOV
//      into glm::perspective's fovy slot. The vertical half-FOV is then derived
//      separately with atan(tan(h) * aspect), so the two do not agree with each
//      other in the usual way. Changing this changes the framing of every shot.
//
//   2. buildPerspectiveMatrix negates the whole of column 1 to flip viewport Y,
//      rather than negating the projection's [1][1] alone.

const std = @import("std");

const linalg = @import("../math/linalg.zig");

const Mat4 = linalg.Mat4;
const Mat4x3 = linalg.Mat4x3;

pub const RendererViewport = struct { // FIXME name
    extent: linalg.UVec2,
    aspect_ratio: f32,
};

pub fn buildRendererViewport(extent: linalg.UVec2) RendererViewport {
    return .{
        .extent = extent,
        .aspect_ratio = @as(f32, @floatFromInt(extent[0])) / @as(f32, @floatFromInt(extent[1])),
    };
}

pub const RendererPerspectiveProjection = struct { // FIXME name
    vs_to_cs_matrix: Mat4, // Commonly projection matrix
    cs_to_vs_matrix: Mat4,

    near_plane_distance: f32,
    far_plane_distance: f32,
    half_fov_horizontal_radian: f32,
    half_fov_vertical_radian: f32,
    use_reverse_z: bool,
};

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

pub fn buildRendererPerspectiveProjection(
    aspect_ratio: f32,
    near_plane_distance: f32,
    far_plane_distance: f32,
    half_fov_horizontal_radian: f32,
    use_reverse_z: bool,
) RendererPerspectiveProjection {
    const projection_matrix = applyReverseZFixup(
        buildPerspectiveMatrix(
            near_plane_distance,
            far_plane_distance,
            aspect_ratio,
            half_fov_horizontal_radian,
        ),
        use_reverse_z,
    );

    return .{
        .vs_to_cs_matrix = projection_matrix,
        .cs_to_vs_matrix = linalg.inverseMat4(projection_matrix),
        .near_plane_distance = near_plane_distance,
        .far_plane_distance = far_plane_distance,
        .half_fov_horizontal_radian = half_fov_horizontal_radian,
        .half_fov_vertical_radian = std.math.atan(@tan(half_fov_horizontal_radian) * aspect_ratio),
        .use_reverse_z = use_reverse_z,
    };
}

pub const RendererPerspectiveCamera = struct { // FIXME name
    vs_to_ws_matrix: Mat4x3,
    ws_to_vs_matrix: Mat4x3, // Commonly view matrix

    viewport: RendererViewport, // FIXME split?
    perspective_projection: RendererPerspectiveProjection,

    ws_to_cs_matrix: Mat4, // Commonly view proj matrix
    cs_to_ws_matrix: Mat4,
};

pub fn buildRendererPerspectiveCamera(
    transform: Mat4x3,
    perspective_projection: RendererPerspectiveProjection,
    viewport: RendererViewport,
) RendererPerspectiveCamera {
    const ws_to_vs_matrix = linalg.mat4x3FromMat4(
        linalg.inverseMat4(linalg.mat4FromMat4x3(transform)),
    );

    return .{
        .vs_to_ws_matrix = transform,
        .ws_to_vs_matrix = ws_to_vs_matrix,
        .viewport = viewport,
        .perspective_projection = perspective_projection,
        .ws_to_cs_matrix = linalg.mulMat4(
            perspective_projection.vs_to_cs_matrix,
            linalg.mat4FromMat4x3(ws_to_vs_matrix),
        ),
        .cs_to_ws_matrix = linalg.mulMat4(
            linalg.mat4FromMat4x3(transform),
            perspective_projection.cs_to_vs_matrix,
        ),
    };
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;
const tolerance = 1e-4;

test "viewport aspect ratio" {
    const viewport = buildRendererViewport(.{ 1920, 1080 });
    try testing.expectApproxEqAbs(@as(f32, 16.0 / 9.0), viewport.aspect_ratio, tolerance);
}

test "projection and its inverse round-trip" {
    const projection = buildRendererPerspectiveProjection(16.0 / 9.0, 0.1, 100.0, std.math.degreesToRadians(45.0), false);

    const round_trip = linalg.mulMat4(projection.vs_to_cs_matrix, projection.cs_to_vs_matrix);

    inline for (0..4) |col| {
        inline for (0..4) |row| {
            const expected: f32 = if (col == row) 1.0 else 0.0;
            try testing.expectApproxEqAbs(expected, round_trip.c[col][row], tolerance);
        }
    }
}

test "viewport Y flip negates the whole of column 1" {
    const aspect_ratio: f32 = 16.0 / 9.0;
    const half_fov = std.math.degreesToRadians(45.0);

    const flipped = buildPerspectiveMatrix(0.1, 100.0, aspect_ratio, half_fov);
    const unflipped = linalg.perspectiveRhZo(half_fov, aspect_ratio, 0.1, 100.0);

    inline for (0..4) |row| {
        try testing.expectApproxEqAbs(-unflipped.c[1][row], flipped.c[1][row], tolerance);
    }

    // No other column is touched.
    inline for (0..4) |row| {
        try testing.expectApproxEqAbs(unflipped.c[0][row], flipped.c[0][row], tolerance);
        try testing.expectApproxEqAbs(unflipped.c[2][row], flipped.c[2][row], tolerance);
    }
}

test "reverse z maps the near plane to 1 and the far plane to 0" {
    const near = 0.1;
    const far = 100.0;
    const projection = buildRendererPerspectiveProjection(1.0, near, far, std.math.degreesToRadians(45.0), true);

    const on_near = linalg.mulMat4Vec4(projection.vs_to_cs_matrix, .{ 0, 0, -near, 1 });
    try testing.expectApproxEqAbs(@as(f32, 1), on_near[2] / on_near[3], tolerance);

    const on_far = linalg.mulMat4Vec4(projection.vs_to_cs_matrix, .{ 0, 0, -far, 1 });
    try testing.expectApproxEqAbs(@as(f32, 0), on_far[2] / on_far[3], tolerance);
}

test "the horizontal half-fov is what reaches the projection" {
    // The quirk: the value passed as half_fov_horizontal_radian lands in
    // glm::perspective's fovy slot, so [1][1] is 1/tan(h/2) — not the value you
    // would get from the derived vertical half-FOV.
    const half_fov_horizontal = std.math.degreesToRadians(60.0);
    const aspect_ratio: f32 = 2.0;

    const projection = buildRendererPerspectiveProjection(aspect_ratio, 0.1, 100.0, half_fov_horizontal, false);

    const expected_11 = 1.0 / @tan(half_fov_horizontal / 2.0);
    // Column 1 is negated by the Y flip.
    try testing.expectApproxEqAbs(-expected_11, projection.vs_to_cs_matrix.c[1][1], tolerance);

    // ...and the vertical half-FOV is derived independently.
    const expected_vertical = std.math.atan(@tan(half_fov_horizontal) * aspect_ratio);
    try testing.expectApproxEqAbs(expected_vertical, projection.half_fov_vertical_radian, tolerance);
}

test "camera view matrix is the inverse of its transform" {
    const transform = linalg.mat4x3FromMat4(
        linalg.inverseMat4(linalg.lookAtRh(.{ -2, 0.8, 0 }, .{ 1, 0.4, 0 }, .{ 0, 1, 0 })),
    );

    const projection = buildRendererPerspectiveProjection(16.0 / 9.0, 0.1, 100.0, std.math.degreesToRadians(45.0), true);
    const camera = buildRendererPerspectiveCamera(transform, projection, buildRendererViewport(.{ 1280, 720 }));

    const round_trip = linalg.mulMat4(
        linalg.mat4FromMat4x3(camera.vs_to_ws_matrix),
        linalg.mat4FromMat4x3(camera.ws_to_vs_matrix),
    );

    inline for (0..4) |col| {
        inline for (0..4) |row| {
            const expected: f32 = if (col == row) 1.0 else 0.0;
            try testing.expectApproxEqAbs(expected, round_trip.c[col][row], tolerance);
        }
    }

    // ws_to_cs and cs_to_ws must undo each other too.
    const cs_round_trip = linalg.mulMat4(camera.ws_to_cs_matrix, camera.cs_to_ws_matrix);
    inline for (0..4) |col| {
        inline for (0..4) |row| {
            const expected: f32 = if (col == row) 1.0 else 0.0;
            try testing.expectApproxEqAbs(expected, cs_round_trip.c[col][row], tolerance);
        }
    }
}
