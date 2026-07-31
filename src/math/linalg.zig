// Hand-rolled replacement for the glm subset the renderer uses.
//
// The storage and indexing conventions are glm's, not something nicer, because
// every ported renderpass and every CPU→GPU struct depends on them:
//
//   * matrices are COLUMN-major in both ordering and storage
//   * `m.c[col][row]` is glm's `m[col][row]`
//   * Mat4x3 is 4 columns of 3 rows (glm::mat4x3), which is HLSL's float3x4
//
// The functions that feed the projection chain (perspectiveRhZo, lookAtRh,
// inverse) are transliterated from glm rather than rewritten, so that floating
// point results match operation-for-operation. GLM_FORCE_DEPTH_ZERO_TO_ONE is
// set for the C++ build (cmake/external/common.cmake), hence the _ZO variants.
//
// v1-only: a rework is expected later. Keep this module small, keep it behind
// one import, and keep the quirks documented.

const std = @import("std");
const math = std.math;

pub const Vec2 = @Vector(2, f32);
pub const Vec3 = @Vector(3, f32);
pub const Vec4 = @Vector(4, f32);

pub const UVec2 = @Vector(2, u32);
pub const UVec3 = @Vector(3, u32);

// --------------------------------------------------------------------------
// Vector helpers
// --------------------------------------------------------------------------

pub fn vec2(x: f32, y: f32) Vec2 {
    return .{ x, y };
}

pub fn vec3(x: f32, y: f32, z: f32) Vec3 {
    return .{ x, y, z };
}

pub fn vec4(x: f32, y: f32, z: f32, w: f32) Vec4 {
    return .{ x, y, z, w };
}

pub fn splat(comptime V: type, value: f32) V {
    return @splat(value);
}

pub fn dot(a: anytype, b: @TypeOf(a)) f32 {
    return @reduce(.Add, a * b);
}

pub fn cross(a: Vec3, b: Vec3) Vec3 {
    return .{
        a[1] * b[2] - b[1] * a[2],
        a[2] * b[0] - b[2] * a[0],
        a[0] * b[1] - b[0] * a[1],
    };
}

pub fn length(v: anytype) f32 {
    return @sqrt(dot(v, v));
}

pub fn normalize(v: anytype) @TypeOf(v) {
    const inv_len: @TypeOf(v) = @splat(1.0 / length(v));
    return v * inv_len;
}

pub fn xyz(v: Vec4) Vec3 {
    return .{ v[0], v[1], v[2] };
}

pub fn vec4FromVec3(v: Vec3, w: f32) Vec4 {
    return .{ v[0], v[1], v[2], w };
}

// --------------------------------------------------------------------------
// Matrices
// --------------------------------------------------------------------------

/// 3 columns of 3 rows.
pub const Mat3 = struct {
    c: [3]Vec3,

    pub const identity: Mat3 = .{ .c = .{
        .{ 1, 0, 0 },
        .{ 0, 1, 0 },
        .{ 0, 0, 1 },
    } };

    pub const zero: Mat3 = .{ .c = .{
        .{ 0, 0, 0 },
        .{ 0, 0, 0 },
        .{ 0, 0, 0 },
    } };
};

/// 4 columns of 4 rows — glm::mat4.
pub const Mat4 = struct {
    c: [4]Vec4,

    pub const identity: Mat4 = .{ .c = .{
        .{ 1, 0, 0, 0 },
        .{ 0, 1, 0, 0 },
        .{ 0, 0, 1, 0 },
        .{ 0, 0, 0, 1 },
    } };

    pub const zero: Mat4 = .{ .c = .{
        .{ 0, 0, 0, 0 },
        .{ 0, 0, 0, 0 },
        .{ 0, 0, 0, 0 },
        .{ 0, 0, 0, 0 },
    } };
};

/// 4 columns of 3 rows — glm::mat4x3, i.e. an affine transform without the
/// implicit bottom row. NOTE: this is HLSL's float3x4, not float4x3.
pub const Mat4x3 = struct {
    c: [4]Vec3,

    pub const identity: Mat4x3 = .{ .c = .{
        .{ 1, 0, 0 },
        .{ 0, 1, 0 },
        .{ 0, 0, 1 },
        .{ 0, 0, 0 },
    } };
};

/// Widens an affine transform to a full 4x4, appending glm's implicit
/// (0, 0, 0, 1) bottom row.
pub fn mat4FromMat4x3(m: Mat4x3) Mat4 {
    return .{ .c = .{
        vec4FromVec3(m.c[0], 0),
        vec4FromVec3(m.c[1], 0),
        vec4FromVec3(m.c[2], 0),
        vec4FromVec3(m.c[3], 1),
    } };
}

/// Drops the bottom row. Mirrors glm's mat4 -> mat4x3 conversion.
pub fn mat4x3FromMat4(m: Mat4) Mat4x3 {
    return .{ .c = .{ xyz(m.c[0]), xyz(m.c[1]), xyz(m.c[2]), xyz(m.c[3]) } };
}

pub fn mat3FromMat4(m: Mat4) Mat3 {
    return .{ .c = .{ xyz(m.c[0]), xyz(m.c[1]), xyz(m.c[2]) } };
}

// --------------------------------------------------------------------------
// Matrix products
// --------------------------------------------------------------------------

pub fn mulMat4Vec4(m: Mat4, v: Vec4) Vec4 {
    const x: Vec4 = @splat(v[0]);
    const y: Vec4 = @splat(v[1]);
    const z: Vec4 = @splat(v[2]);
    const w: Vec4 = @splat(v[3]);

    return m.c[0] * x + m.c[1] * y + m.c[2] * z + m.c[3] * w;
}

/// glm's `fmat4x3 * fvec4`, which yields an fvec3. Equivalent to promoting to a
/// mat4 (implicit bottom row 0,0,0,1) and dropping w.
pub fn mulMat4x3Vec4(m: Mat4x3, v: Vec4) Vec3 {
    return m.c[0] * splat(Vec3, v[0]) +
        m.c[1] * splat(Vec3, v[1]) +
        m.c[2] * splat(Vec3, v[2]) +
        m.c[3] * splat(Vec3, v[3]);
}

pub fn mulMat3Vec3(m: Mat3, v: Vec3) Vec3 {
    const x: Vec3 = @splat(v[0]);
    const y: Vec3 = @splat(v[1]);
    const z: Vec3 = @splat(v[2]);

    return m.c[0] * x + m.c[1] * y + m.c[2] * z;
}

pub fn mulMat4(a: Mat4, b: Mat4) Mat4 {
    return .{ .c = .{
        mulMat4Vec4(a, b.c[0]),
        mulMat4Vec4(a, b.c[1]),
        mulMat4Vec4(a, b.c[2]),
        mulMat4Vec4(a, b.c[3]),
    } };
}

pub fn mulMat3(a: Mat3, b: Mat3) Mat3 {
    return .{ .c = .{
        mulMat3Vec3(a, b.c[0]),
        mulMat3Vec3(a, b.c[1]),
        mulMat3Vec3(a, b.c[2]),
    } };
}

pub fn transposeMat4(m: Mat4) Mat4 {
    return .{ .c = .{
        .{ m.c[0][0], m.c[1][0], m.c[2][0], m.c[3][0] },
        .{ m.c[0][1], m.c[1][1], m.c[2][1], m.c[3][1] },
        .{ m.c[0][2], m.c[1][2], m.c[2][2], m.c[3][2] },
        .{ m.c[0][3], m.c[1][3], m.c[2][3], m.c[3][3] },
    } };
}

pub fn transposeMat3(m: Mat3) Mat3 {
    return .{ .c = .{
        .{ m.c[0][0], m.c[1][0], m.c[2][0] },
        .{ m.c[0][1], m.c[1][1], m.c[2][1] },
        .{ m.c[0][2], m.c[1][2], m.c[2][2] },
    } };
}

pub fn scaleMat4(m: Mat4, factor: f32) Mat4 {
    const s: Vec4 = @splat(factor);
    return .{ .c = .{ m.c[0] * s, m.c[1] * s, m.c[2] * s, m.c[3] * s } };
}

// --------------------------------------------------------------------------
// Inverse — transliterated from glm/detail/func_matrix.inl
// --------------------------------------------------------------------------

pub fn inverseMat4(m: Mat4) Mat4 {
    const coef00 = m.c[2][2] * m.c[3][3] - m.c[3][2] * m.c[2][3];
    const coef02 = m.c[1][2] * m.c[3][3] - m.c[3][2] * m.c[1][3];
    const coef03 = m.c[1][2] * m.c[2][3] - m.c[2][2] * m.c[1][3];

    const coef04 = m.c[2][1] * m.c[3][3] - m.c[3][1] * m.c[2][3];
    const coef06 = m.c[1][1] * m.c[3][3] - m.c[3][1] * m.c[1][3];
    const coef07 = m.c[1][1] * m.c[2][3] - m.c[2][1] * m.c[1][3];

    const coef08 = m.c[2][1] * m.c[3][2] - m.c[3][1] * m.c[2][2];
    const coef10 = m.c[1][1] * m.c[3][2] - m.c[3][1] * m.c[1][2];
    const coef11 = m.c[1][1] * m.c[2][2] - m.c[2][1] * m.c[1][2];

    const coef12 = m.c[2][0] * m.c[3][3] - m.c[3][0] * m.c[2][3];
    const coef14 = m.c[1][0] * m.c[3][3] - m.c[3][0] * m.c[1][3];
    const coef15 = m.c[1][0] * m.c[2][3] - m.c[2][0] * m.c[1][3];

    const coef16 = m.c[2][0] * m.c[3][2] - m.c[3][0] * m.c[2][2];
    const coef18 = m.c[1][0] * m.c[3][2] - m.c[3][0] * m.c[1][2];
    const coef19 = m.c[1][0] * m.c[2][2] - m.c[2][0] * m.c[1][2];

    const coef20 = m.c[2][0] * m.c[3][1] - m.c[3][0] * m.c[2][1];
    const coef22 = m.c[1][0] * m.c[3][1] - m.c[3][0] * m.c[1][1];
    const coef23 = m.c[1][0] * m.c[2][1] - m.c[2][0] * m.c[1][1];

    const fac0 = Vec4{ coef00, coef00, coef02, coef03 };
    const fac1 = Vec4{ coef04, coef04, coef06, coef07 };
    const fac2 = Vec4{ coef08, coef08, coef10, coef11 };
    const fac3 = Vec4{ coef12, coef12, coef14, coef15 };
    const fac4 = Vec4{ coef16, coef16, coef18, coef19 };
    const fac5 = Vec4{ coef20, coef20, coef22, coef23 };

    const vec_0 = Vec4{ m.c[1][0], m.c[0][0], m.c[0][0], m.c[0][0] };
    const vec_1 = Vec4{ m.c[1][1], m.c[0][1], m.c[0][1], m.c[0][1] };
    const vec_2 = Vec4{ m.c[1][2], m.c[0][2], m.c[0][2], m.c[0][2] };
    const vec_3 = Vec4{ m.c[1][3], m.c[0][3], m.c[0][3], m.c[0][3] };

    const inv0 = vec_1 * fac0 - vec_2 * fac1 + vec_3 * fac2;
    const inv1 = vec_0 * fac0 - vec_2 * fac3 + vec_3 * fac4;
    const inv2 = vec_0 * fac1 - vec_1 * fac3 + vec_3 * fac5;
    const inv3 = vec_0 * fac2 - vec_1 * fac4 + vec_2 * fac5;

    const sign_a = Vec4{ 1, -1, 1, -1 };
    const sign_b = Vec4{ -1, 1, -1, 1 };

    const inverse = Mat4{ .c = .{
        inv0 * sign_a,
        inv1 * sign_b,
        inv2 * sign_a,
        inv3 * sign_b,
    } };

    const row0 = Vec4{ inverse.c[0][0], inverse.c[1][0], inverse.c[2][0], inverse.c[3][0] };

    const dot0 = m.c[0] * row0;
    const dot1 = (dot0[0] + dot0[1]) + (dot0[2] + dot0[3]);

    return scaleMat4(inverse, 1.0 / dot1);
}

// --------------------------------------------------------------------------
// Projections and views
// --------------------------------------------------------------------------

/// glm::perspective under GLM_FORCE_DEPTH_ZERO_TO_ONE, i.e. perspectiveRH_ZO.
///
/// NOTE: Camera.cpp passes the *horizontal* half-FOV into the fovy slot. That
/// is a quirk of the engine, not of this function — see buildPerspectiveMatrix.
pub fn perspectiveRhZo(fovy: f32, aspect: f32, z_near: f32, z_far: f32) Mat4 {
    std.debug.assert(@abs(aspect - math.floatEps(f32)) > 0.0);

    const tan_half_fovy = @tan(fovy / 2.0);

    var result = Mat4.zero;
    result.c[0][0] = 1.0 / (aspect * tan_half_fovy);
    result.c[1][1] = 1.0 / tan_half_fovy;
    result.c[2][2] = z_far / (z_near - z_far);
    result.c[2][3] = -1.0;
    result.c[3][2] = -(z_far * z_near) / (z_far - z_near);

    return result;
}

/// glm::lookAtRH.
pub fn lookAtRh(eye: Vec3, center: Vec3, up: Vec3) Mat4 {
    const f = normalize(center - eye);
    const s = normalize(cross(f, up));
    const u = cross(s, f);

    var result = Mat4.identity;
    result.c[0][0] = s[0];
    result.c[1][0] = s[1];
    result.c[2][0] = s[2];
    result.c[0][1] = u[0];
    result.c[1][1] = u[1];
    result.c[2][1] = u[2];
    result.c[0][2] = -f[0];
    result.c[1][2] = -f[1];
    result.c[2][2] = -f[2];
    result.c[3][0] = -dot(s, eye);
    result.c[3][1] = -dot(u, eye);
    result.c[3][2] = dot(f, eye);

    return result;
}

/// glm::translate(mat4(1), v)
pub fn translate(v: Vec3) Mat4 {
    var result = Mat4.identity;
    result.c[3] = vec4FromVec3(v, 1.0);
    return result;
}

/// glm::scale(mat4(1), v)
pub fn scale(v: Vec3) Mat4 {
    var result = Mat4.identity;
    result.c[0][0] = v[0];
    result.c[1][1] = v[1];
    result.c[2][2] = v[2];
    return result;
}

// --------------------------------------------------------------------------
// Quaternions
// --------------------------------------------------------------------------

/// Component order matches glm's storage, not its constructor: glm::qua is
/// declared `{x, y, z, w}` but constructed as `qua(w, x, y, z)`.
pub const Quat = struct {
    x: f32,
    y: f32,
    z: f32,
    w: f32,

    pub const identity: Quat = .{ .x = 0, .y = 0, .z = 0, .w = 1 };
};

/// glm::angleAxis
pub fn angleAxis(angle: f32, axis: Vec3) Quat {
    const s = @sin(angle * 0.5);

    return .{
        .x = axis[0] * s,
        .y = axis[1] * s,
        .z = axis[2] * s,
        .w = @cos(angle * 0.5),
    };
}

/// glm::qua operator*(qua, qua) — Hamilton product, `a` applied after `b`.
pub fn mulQuat(a: Quat, b: Quat) Quat {
    return .{
        .w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        .x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        .y = a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
        .z = a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x,
    };
}

/// glm::qua operator*(qua, vec3) — rotates `v` by `q`.
pub fn rotateVec3(q: Quat, v: Vec3) Vec3 {
    const quat_vector = Vec3{ q.x, q.y, q.z };
    const uv = cross(quat_vector, v);
    const uuv = cross(quat_vector, uv);

    return v + ((uv * splat(Vec3, q.w)) + uuv) * splat(Vec3, 2.0);
}

/// glm::rotate(mat4(1), angle, axis). glm normalizes the axis internally.
pub fn rotate(angle: f32, axis: Vec3) Mat4 {
    return quatToMat4(angleAxis(angle, normalize(axis)));
}

/// glm::mat3_cast
pub fn quatToMat3(q: Quat) Mat3 {
    const qxx = q.x * q.x;
    const qyy = q.y * q.y;
    const qzz = q.z * q.z;
    const qxz = q.x * q.z;
    const qxy = q.x * q.y;
    const qyz = q.y * q.z;
    const qwx = q.w * q.x;
    const qwy = q.w * q.y;
    const qwz = q.w * q.z;

    var result = Mat3.identity;

    result.c[0][0] = 1.0 - 2.0 * (qyy + qzz);
    result.c[0][1] = 2.0 * (qxy + qwz);
    result.c[0][2] = 2.0 * (qxz - qwy);

    result.c[1][0] = 2.0 * (qxy - qwz);
    result.c[1][1] = 1.0 - 2.0 * (qxx + qzz);
    result.c[1][2] = 2.0 * (qyz + qwx);

    result.c[2][0] = 2.0 * (qxz + qwy);
    result.c[2][1] = 2.0 * (qyz - qwx);
    result.c[2][2] = 1.0 - 2.0 * (qxx + qyy);

    return result;
}

/// glm::mat4_cast
pub fn quatToMat4(q: Quat) Mat4 {
    const m = quatToMat3(q);

    return .{ .c = .{
        vec4FromVec3(m.c[0], 0),
        vec4FromVec3(m.c[1], 0),
        vec4FromVec3(m.c[2], 0),
        .{ 0, 0, 0, 1 },
    } };
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;
const tolerance = 1e-5;

fn expectMat4Approx(expected: Mat4, actual: Mat4) !void {
    inline for (0..4) |col| {
        inline for (0..4) |row| {
            testing.expectApproxEqAbs(expected.c[col][row], actual.c[col][row], tolerance) catch |err| {
                std.debug.print("mismatch at c[{}][{}]\n", .{ col, row });
                return err;
            };
        }
    }
}

test "identity is a multiplicative identity" {
    const m = Mat4{ .c = .{
        .{ 1, 2, 3, 4 },
        .{ 5, 6, 7, 8 },
        .{ 9, 10, 11, 12 },
        .{ 13, 14, 15, 16 },
    } };

    try expectMat4Approx(m, mulMat4(m, Mat4.identity));
    try expectMat4Approx(m, mulMat4(Mat4.identity, m));
}

test "column-major indexing matches glm" {
    // glm's m[3] is the translation column of an affine transform.
    const t = translate(.{ 1, 2, 3 });

    try testing.expectEqual(@as(f32, 1), t.c[3][0]);
    try testing.expectEqual(@as(f32, 2), t.c[3][1]);
    try testing.expectEqual(@as(f32, 3), t.c[3][2]);
    try testing.expectEqual(@as(f32, 1), t.c[3][3]);

    // Translating a point must move it.
    const p = mulMat4Vec4(t, .{ 10, 20, 30, 1 });
    try testing.expectEqual(Vec4{ 11, 22, 33, 1 }, p);

    // Translating a direction (w = 0) must not.
    const d = mulMat4Vec4(t, .{ 10, 20, 30, 0 });
    try testing.expectEqual(Vec4{ 10, 20, 30, 0 }, d);
}

test "inverse of a general matrix" {
    // Deliberately not orthonormal, so transpose-as-inverse would fail.
    const m = Mat4{ .c = .{
        .{ 2, 0.5, -1, 0 },
        .{ 1, 3, 0.25, 0 },
        .{ -0.5, 1, 4, 0 },
        .{ 7, -2, 3, 1 },
    } };

    try expectMat4Approx(Mat4.identity, mulMat4(m, inverseMat4(m)));
    try expectMat4Approx(Mat4.identity, mulMat4(inverseMat4(m), m));
}

test "perspectiveRhZo maps the near and far planes to 0 and 1" {
    const near = 0.1;
    const far = 100.0;
    const projection = perspectiveRhZo(math.degreesToRadians(60.0), 16.0 / 9.0, near, far);

    // A point on the near plane sits at depth 0 after the perspective divide,
    // a point on the far plane at depth 1. This is what ZO means, and getting
    // it wrong (the GL-style -1..1 convention) would break every depth test.
    const on_near = mulMat4Vec4(projection, .{ 0, 0, -near, 1 });
    try testing.expectApproxEqAbs(@as(f32, 0), on_near[2] / on_near[3], tolerance);

    const on_far = mulMat4Vec4(projection, .{ 0, 0, -far, 1 });
    try testing.expectApproxEqAbs(@as(f32, 1), on_far[2] / on_far[3], tolerance);

    // Right-handed: the viewer looks down -Z, so +Z is behind and w must be
    // negative there.
    const behind = mulMat4Vec4(projection, .{ 0, 0, 1, 1 });
    try testing.expect(behind[3] < 0);
}

test "perspectiveRhZo golden values" {
    const projection = perspectiveRhZo(math.degreesToRadians(90.0), 2.0, 1.0, 11.0);

    // tan(45 deg) == 1, so the diagonal terms are exactly 1/aspect and 1.
    try testing.expectApproxEqAbs(@as(f32, 0.5), projection.c[0][0], tolerance);
    try testing.expectApproxEqAbs(@as(f32, 1.0), projection.c[1][1], tolerance);
    try testing.expectApproxEqAbs(@as(f32, -1.1), projection.c[2][2], tolerance);
    try testing.expectApproxEqAbs(@as(f32, -1.0), projection.c[2][3], tolerance);
    try testing.expectApproxEqAbs(@as(f32, -1.1), projection.c[3][2], tolerance);

    // Everything else is zero.
    try testing.expectEqual(@as(f32, 0), projection.c[0][1]);
    try testing.expectEqual(@as(f32, 0), projection.c[3][3]);
}

test "lookAtRh builds a world-to-view transform" {
    const eye = Vec3{ 0, 0, 5 };
    const center = Vec3{ 0, 0, 0 };
    const up = Vec3{ 0, 1, 0 };

    const view = lookAtRh(eye, center, up);

    // The eye maps to the view-space origin.
    const eye_vs = mulMat4Vec4(view, vec4FromVec3(eye, 1));
    try testing.expectApproxEqAbs(@as(f32, 0), eye_vs[0], tolerance);
    try testing.expectApproxEqAbs(@as(f32, 0), eye_vs[1], tolerance);
    try testing.expectApproxEqAbs(@as(f32, 0), eye_vs[2], tolerance);

    // The target sits straight ahead, down -Z.
    const center_vs = mulMat4Vec4(view, vec4FromVec3(center, 1));
    try testing.expectApproxEqAbs(@as(f32, 0), center_vs[0], tolerance);
    try testing.expectApproxEqAbs(@as(f32, 0), center_vs[1], tolerance);
    try testing.expectApproxEqAbs(@as(f32, -5), center_vs[2], tolerance);
}

test "lookAtRh inverse round-trips" {
    const view = lookAtRh(.{ 3, 4, 5 }, .{ 1, 0, -2 }, .{ 0, 1, 0 });
    try expectMat4Approx(Mat4.identity, mulMat4(view, inverseMat4(view)));
}

test "angleAxis and quatToMat4 agree with a hand-built rotation" {
    // A quarter turn about +Y takes +X to -Z in a right-handed system.
    const q = angleAxis(math.pi / 2.0, .{ 0, 1, 0 });
    const m = quatToMat4(q);

    const rotated = mulMat4Vec4(m, .{ 1, 0, 0, 1 });
    try testing.expectApproxEqAbs(@as(f32, 0), rotated[0], tolerance);
    try testing.expectApproxEqAbs(@as(f32, 0), rotated[1], tolerance);
    try testing.expectApproxEqAbs(@as(f32, -1), rotated[2], tolerance);

    // The identity quaternion is the identity matrix.
    try expectMat4Approx(Mat4.identity, quatToMat4(Quat.identity));
}

test "angleAxis composes to a full turn" {
    const q = angleAxis(math.pi / 2.0, normalize(Vec3{ 1, 2, 3 }));
    const m = quatToMat4(q);
    const four_turns = mulMat4(mulMat4(m, m), mulMat4(m, m));

    try expectMat4Approx(Mat4.identity, four_turns);
}

test "mat4x3 round-trips through mat4" {
    const m = Mat4x3{ .c = .{
        .{ 1, 2, 3 },
        .{ 4, 5, 6 },
        .{ 7, 8, 9 },
        .{ 10, 11, 12 },
    } };

    const widened = mat4FromMat4x3(m);

    // The implicit bottom row is (0, 0, 0, 1).
    try testing.expectEqual(@as(f32, 0), widened.c[0][3]);
    try testing.expectEqual(@as(f32, 0), widened.c[1][3]);
    try testing.expectEqual(@as(f32, 0), widened.c[2][3]);
    try testing.expectEqual(@as(f32, 1), widened.c[3][3]);

    const narrowed = mat4x3FromMat4(widened);
    for (0..4) |col| {
        try testing.expectEqual(m.c[col], narrowed.c[col]);
    }
}

test "transpose is an involution" {
    const m = Mat4{ .c = .{
        .{ 1, 2, 3, 4 },
        .{ 5, 6, 7, 8 },
        .{ 9, 10, 11, 12 },
        .{ 13, 14, 15, 16 },
    } };

    try expectMat4Approx(m, transposeMat4(transposeMat4(m)));

    // Transposing swaps rows and columns, in glm's indexing.
    try testing.expectEqual(m.c[0][1], transposeMat4(m).c[1][0]);
}

test "cross and dot follow the right-hand rule" {
    try testing.expectEqual(Vec3{ 0, 0, 1 }, cross(.{ 1, 0, 0 }, .{ 0, 1, 0 }));
    try testing.expectEqual(@as(f32, 0), dot(Vec3{ 1, 0, 0 }, Vec3{ 0, 1, 0 }));
    try testing.expectApproxEqAbs(@as(f32, 1), length(normalize(Vec3{ 1, 2, 3 })), tolerance);
}
