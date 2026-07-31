// Port of src/renderer/hlsl/Types.inl + src/renderer/shader/shared_types.hlsl
//
// These are the CPU-side mirrors of the HLSL interop types. Their whole reason
// to exist is memory layout, so the rules are worth stating plainly:
//
//   * float2 is 8 bytes, aligned to 8
//   * float3 is 12 bytes, aligned to 4 — it deliberately has NO 16-byte
//     alignment, because C++ cannot express "align 16, size 12"
//   * float3_pad is 16 bytes, aligned to 16, and is what matrices are built
//     from so that element[n] indexing works
//   * float4 is 16 bytes, aligned to 16
//   * matrices are COLUMN-major storage (REAPER_HLSL_INTEROP_MATRIX_STORAGE ==
//     REAPER_COL_MAJOR), so element[i] is column i
//
// Beware: a 4x3 matrix in glm is a 3x4 matrix in HLSL. Matrix3x4 here follows
// the HLSL name and stores 4 columns of 3 rows.
//
// Zig has no struct-level `align()`, so the alignment is carried by the first
// field, which produces the same layout as C++'s `alignas` on the struct.

const std = @import("std");

const linalg = @import("../../math/linalg.zig");

pub const Float = f32;
pub const Int = i32;
pub const Uint = u32;

// --------------------------------------------------------------------------
// Vectors
// --------------------------------------------------------------------------

pub fn Vector2(comptime T: type) type {
    return extern struct {
        x: T align(2 * @sizeOf(T)) = 0,
        y: T = 0,
    };
}

/// NOTE: no over-alignment, matching hlsl_vector3's comment that 16-byte
/// alignment and a 12-byte size cannot coexist.
pub fn Vector3(comptime T: type) type {
    return extern struct {
        x: T = 0,
        y: T = 0,
        z: T = 0,
    };
}

/// The padded flavour used inside matrices.
pub fn Vector3Pad(comptime T: type) type {
    return extern struct {
        x: T align(4 * @sizeOf(T)) = 0,
        y: T = 0,
        z: T = 0,
        _pad: T = 0,
    };
}

pub fn Vector4(comptime T: type) type {
    return extern struct {
        x: T align(4 * @sizeOf(T)) = 0,
        y: T = 0,
        z: T = 0,
        w: T = 0,
    };
}

// --------------------------------------------------------------------------
// Matrices (column-major storage)
// --------------------------------------------------------------------------

pub fn Matrix3x3(comptime T: type) type {
    return extern struct {
        element: [3]Vector3Pad(T) = @splat(.{}),
    };
}

/// HLSL float3x4: 3 rows, 4 columns. Column-major storage means 4 padded
/// 3-vectors, one per column.
pub fn Matrix3x4(comptime T: type) type {
    return extern struct {
        element: [4]Vector3Pad(T) = @splat(.{}),
    };
}

pub fn Matrix4x4(comptime T: type) type {
    return extern struct {
        element: [4]Vector4(T) = @splat(.{}),
    };
}

// --------------------------------------------------------------------------
// The hlsl_* aliases from shared_types.hlsl
// --------------------------------------------------------------------------

pub const Float2 = Vector2(f32);
pub const Float3 = Vector3(f32);
pub const Float3Pad = Vector3Pad(f32);
pub const Float4 = Vector4(f32);

pub const Int2 = Vector2(i32);
pub const Int3 = Vector3(i32);
pub const Int4 = Vector4(i32);

pub const Uint2 = Vector2(u32);
pub const Uint3 = Vector3(u32);
pub const Uint4 = Vector4(u32);

pub const Float3x3 = Matrix3x3(f32);
pub const Float3x4 = Matrix3x4(f32);
pub const Float4x4 = Matrix4x4(f32);

pub const Int3x3 = Matrix3x3(i32);
pub const Int3x4 = Matrix3x4(i32);
pub const Int4x4 = Matrix4x4(i32);

pub const Uint3x3 = Matrix3x3(u32);
pub const Uint3x4 = Matrix3x4(u32);
pub const Uint4x4 = Matrix4x4(u32);

// --------------------------------------------------------------------------
// Conversion from the math layer
// --------------------------------------------------------------------------

pub fn float2(v: linalg.Vec2) Float2 {
    return .{ .x = v[0], .y = v[1] };
}

pub fn float3(v: linalg.Vec3) Float3 {
    return .{ .x = v[0], .y = v[1], .z = v[2] };
}

pub fn float3Pad(v: linalg.Vec3) Float3Pad {
    return .{ .x = v[0], .y = v[1], .z = v[2] };
}

pub fn float4(v: linalg.Vec4) Float4 {
    return .{ .x = v[0], .y = v[1], .z = v[2], .w = v[3] };
}

pub fn uint2(v: linalg.UVec2) Uint2 {
    return .{ .x = v[0], .y = v[1] };
}

pub fn uint3(v: linalg.UVec3) Uint3 {
    return .{ .x = v[0], .y = v[1], .z = v[2] };
}

/// element[i] = column i.
pub fn float3x3(m: linalg.Mat3) Float3x3 {
    return .{ .element = .{ float3Pad(m.c[0]), float3Pad(m.c[1]), float3Pad(m.c[2]) } };
}

/// glm::mat4x3 (4 columns of 3 rows) becomes HLSL float3x4.
pub fn float3x4(m: linalg.Mat4x3) Float3x4 {
    return .{ .element = .{
        float3Pad(m.c[0]),
        float3Pad(m.c[1]),
        float3Pad(m.c[2]),
        float3Pad(m.c[3]),
    } };
}

pub fn float4x4(m: linalg.Mat4) Float4x4 {
    return .{ .element = .{
        float4(m.c[0]),
        float4(m.c[1]),
        float4(m.c[2]),
        float4(m.c[3]),
    } };
}

// --------------------------------------------------------------------------
// Layout checking
// --------------------------------------------------------------------------

/// Asserts that `T` occupies exactly `expected_size` bytes with its fields back
/// to back and nothing implicit anywhere — no interior padding, no trailing
/// padding.
///
/// These structs keep their HLSL counterparts in step by carrying explicit
/// `_pad` members, so any padding the compiler had to invent means a field is
/// missing, misordered, or the wrong type, and the GPU would read garbage from
/// that offset onwards. Checking it this way means no offset has to be worked
/// out by hand.
///
/// Checking the declared bytes matters as much as checking `@sizeOf`: dropping
/// a trailing `_pad` member usually leaves `@sizeOf` unchanged, because the
/// struct just rounds back up to its own alignment. Only the field sum notices.
pub fn assertLayout(comptime T: type, comptime expected_size: usize) void {
    assertLayoutPadded(T, expected_size, expected_size);
}

/// Same as assertLayout, for the few structs whose trailing padding is real:
/// one ending in a `uint2` rounds up to 8-byte alignment whatever we do. Spell
/// out both numbers so the gap is visible at the call site rather than implied.
pub fn assertLayoutPadded(
    comptime T: type,
    comptime declared_bytes: usize,
    comptime expected_size: usize,
) void {
    comptime {
        var expected_offset: usize = 0;

        for (@typeInfo(T).@"struct".fields) |field| {
            const actual_offset = @offsetOf(T, field.name);

            if (actual_offset != expected_offset) {
                @compileError(std.fmt.comptimePrint(
                    "{s}.{s} is at offset {d}, expected {d}: the compiler inserted padding, so a field is missing or misordered",
                    .{ @typeName(T), field.name, actual_offset, expected_offset },
                ));
            }

            expected_offset += @sizeOf(field.type);
        }

        if (expected_offset != declared_bytes) {
            @compileError(std.fmt.comptimePrint(
                "{s} declares {d} bytes of fields, expected {d}",
                .{ @typeName(T), expected_offset, declared_bytes },
            ));
        }

        if (@sizeOf(T) != expected_size) {
            @compileError(std.fmt.comptimePrint(
                "@sizeOf({s}) is {d}, expected {d}",
                .{ @typeName(T), @sizeOf(T), expected_size },
            ));
        }
    }
}

// --------------------------------------------------------------------------
// Tests — ported from src/renderer/test/hlsl/{float_vector,float_matrix,struct}.cpp
// --------------------------------------------------------------------------

const testing = std.testing;

test "HLSL float vector types" {
    try testing.expectEqual(1 * @sizeOf(f32), @sizeOf(Float));
    try testing.expectEqual(1 * @sizeOf(f32), @alignOf(Float));

    try testing.expectEqual(2 * @sizeOf(f32), @sizeOf(Float2));
    try testing.expectEqual(2 * @sizeOf(f32), @alignOf(Float2));

    // 3 floats wide, but NOT 4-float aligned — this is the awkward one.
    try testing.expectEqual(3 * @sizeOf(f32), @sizeOf(Float3));
    try testing.expectEqual(1 * @sizeOf(f32), @alignOf(Float3));

    try testing.expectEqual(4 * @sizeOf(f32), @sizeOf(Float3Pad));
    try testing.expectEqual(4 * @sizeOf(f32), @alignOf(Float3Pad));

    try testing.expectEqual(4 * @sizeOf(f32), @sizeOf(Float4));
    try testing.expectEqual(4 * @sizeOf(f32), @alignOf(Float4));

    const zeroed = Float4{};
    try testing.expectEqual(@as(f32, 0), zeroed.x);
    try testing.expectEqual(@as(f32, 0), zeroed.w);

    const initialized = float4(.{ 1, 2, 3, 4 });
    try testing.expectEqual(@as(f32, 1), initialized.x);
    try testing.expectEqual(@as(f32, 2), initialized.y);
    try testing.expectEqual(@as(f32, 3), initialized.z);
    try testing.expectEqual(@as(f32, 4), initialized.w);
}

test "HLSL float matrix types" {
    try testing.expectEqual(4 * @sizeOf(f32), @alignOf(Float3x3));
    try testing.expectEqual(12 * @sizeOf(f32), @sizeOf(Float3x3));

    // Column-major storage makes this 16 floats, not 12.
    try testing.expectEqual(4 * @sizeOf(f32), @alignOf(Float3x4));
    try testing.expectEqual(16 * @sizeOf(f32), @sizeOf(Float3x4));

    try testing.expectEqual(4 * @sizeOf(f32), @alignOf(Float4x4));
    try testing.expectEqual(16 * @sizeOf(f32), @sizeOf(Float4x4));

    const identity = float4x4(linalg.Mat4.identity);

    try testing.expectEqual(@as(f32, 1), identity.element[0].x);
    try testing.expectEqual(@as(f32, 1), identity.element[1].y);
    try testing.expectEqual(@as(f32, 1), identity.element[2].z);
    try testing.expectEqual(@as(f32, 1), identity.element[3].w);

    try testing.expectEqual(@as(f32, 0), identity.element[0].y);
    try testing.expectEqual(@as(f32, 0), identity.element[0].z);
    try testing.expectEqual(@as(f32, 0), identity.element[2].x);
    try testing.expectEqual(@as(f32, 0), identity.element[2].y);
    try testing.expectEqual(@as(f32, 0), identity.element[3].z);

    const identity3x3 = float3x3(linalg.Mat3.identity);
    try testing.expectEqual(@as(f32, 1), identity3x3.element[0].x);
    try testing.expectEqual(@as(f32, 1), identity3x3.element[1].y);
    try testing.expectEqual(@as(f32, 1), identity3x3.element[2].z);
    try testing.expectEqual(@as(f32, 0), identity3x3.element[0].y);

    const identity3x4 = float3x4(linalg.Mat4x3.identity);
    try testing.expectEqual(@as(f32, 1), identity3x4.element[0].x);
    try testing.expectEqual(@as(f32, 1), identity3x4.element[1].y);
    try testing.expectEqual(@as(f32, 1), identity3x4.element[2].z);
    try testing.expectEqual(@as(f32, 0), identity3x4.element[3].x);
}

test "HLSL structs" {
    const vector4_size = 16;

    const Test1 = extern struct { a: Float, b: Float, c: Float, d: Float };
    try testing.expectEqual(vector4_size, @sizeOf(Test1));

    const Test2 = extern struct { a: Int2, b: Float2 };
    try testing.expectEqual(vector4_size, @sizeOf(Test2));

    const Test3 = extern struct { a: Uint4, b: Float4 };
    try testing.expectEqual(2 * vector4_size, @sizeOf(Test3));

    const Test4 = extern struct { b: Float, c: Float, d: Float2 };
    try testing.expectEqual(vector4_size, @sizeOf(Test4));

    const Test5 = extern struct { b: Float4x4, c: Float };
    try testing.expectEqual(5 * vector4_size, @sizeOf(Test5));

    const Test6 = extern struct {
        c: Float2,
        d: Float,
        f: Float,
        b: Float3x3,
        g: Float3,
        h: Float,
        a: Float4x4,
    };
    try testing.expectEqual(9 * vector4_size, @sizeOf(Test6));
}

test "assertLayout accepts a tightly packed struct" {
    const Packed = extern struct { a: Float3, b: Float };
    assertLayout(Packed, 16);
}
