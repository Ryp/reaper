// Port of src/mesh/Mesh.{h,cpp}

const std = @import("std");

const linalg = @import("../math/linalg.zig");

/// Matches the C++ struct byte for byte: the padding is explicit because this
/// is uploaded straight to the GPU.
pub const VertexAttributes = extern struct {
    normal: [3]f32 = .{ 0, 0, 0 },
    _pad0: u32 = 0,
    uv: [2]f32 = .{ 0, 0 },
    _pad1: u32 = 0,
    _pad2: u32 = 0,
    /// GLTF uses w for the bitangent sign (-1.0 or 1.0)
    tangent: [4]f32 = .{ 0, 0, 0, 0 },
};

comptime {
    std.debug.assert(@sizeOf(VertexAttributes) == 48);
    std.debug.assert(@offsetOf(VertexAttributes, "uv") == 16);
    std.debug.assert(@offsetOf(VertexAttributes, "tangent") == 32);
}

pub const Mesh = struct {
    indexes: std.ArrayList(u32) = .empty,

    // Vertex data
    positions: std.ArrayList([3]f32) = .empty,
    attributes: std.ArrayList(VertexAttributes) = .empty,

    pub fn deinit(self: *Mesh, allocator: std.mem.Allocator) void {
        self.indexes.deinit(allocator);
        self.positions.deinit(allocator);
        self.attributes.deinit(allocator);
    }

    pub fn vertexCount(self: Mesh) usize {
        return self.positions.items.len;
    }

    pub fn indexCount(self: Mesh) usize {
        return self.indexes.items.len;
    }
};

pub fn vec3FromArray(v: [3]f32) linalg.Vec3 {
    return .{ v[0], v[1], v[2] };
}
