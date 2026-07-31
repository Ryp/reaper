// glTF loading — the Zig side of the cgltf glue in GameLoop.cpp.
//
// cgltf owns the parsed document and the decoded buffer bytes; this module just
// walks it. The C++ reads attributes back through `accessor.stride` rather than
// assuming tight packing, which is what makes interleaved buffers work, so the
// copy here is strided the same way.

const std = @import("std");

const mesh_module = @import("mesh.zig");

pub const c = @cImport({
    @cInclude("cgltf.h");
});

const log = std.log.scoped(.game);

/// The four maps MeshMaterial samples, as indices into the document's image
/// array. Textures are uploaded in document order, so the index doubles as the
/// offset into the allocated texture handle span.
pub const Material = struct {
    base_color_image: u32,
    metal_roughness_image: u32,
    normal_image: u32,
    ao_image: u32,
};

pub const Gltf = struct {
    data: *c.cgltf_data,
    /// Directory the file lives in, so image URIs can be resolved against it.
    base_path: []const u8,

    allocator: std.mem.Allocator,

    pub fn deinit(self: *Gltf) void {
        c.cgltf_free(self.data);
        self.allocator.free(self.base_path);
        self.* = undefined;
    }

    pub fn imageCount(self: *const Gltf) u32 {
        return @intCast(self.data.images_count);
    }

    pub fn materialCount(self: *const Gltf) u32 {
        return @intCast(self.data.materials_count);
    }

    pub fn meshCount(self: *const Gltf) u32 {
        return @intCast(self.data.meshes_count);
    }

    /// Caller owns the returned path.
    pub fn imagePath(self: *const Gltf, index: u32, allocator: std.mem.Allocator) ![]const u8 {
        const uri = self.data.images[index].uri orelse return error.GltfImageHasNoUri;

        return std.fs.path.join(allocator, &.{ self.base_path, std.mem.span(uri) });
    }

    pub fn material(self: *const Gltf, index: u32) !Material {
        const gltf_material = self.data.materials[index];

        return .{
            .base_color_image = try self.imageIndex(gltf_material.pbr_metallic_roughness.base_color_texture),
            .metal_roughness_image = try self.imageIndex(gltf_material.pbr_metallic_roughness.metallic_roughness_texture),
            .normal_image = try self.imageIndex(gltf_material.normal_texture),
            .ao_image = try self.imageIndex(gltf_material.occlusion_texture),
        };
    }

    /// The C++ recovers the image index by pointer arithmetic against
    /// `data->images`; cgltf lays them out contiguously so that is well defined.
    fn imageIndex(self: *const Gltf, texture_view: c.cgltf_texture_view) !u32 {
        const texture = texture_view.texture orelse return error.GltfMaterialMissingTexture;
        const image = texture.*.image orelse return error.GltfTextureMissingImage;

        return @intCast((@intFromPtr(image) - @intFromPtr(self.data.images)) / @sizeOf(c.cgltf_image));
    }

    /// Which material (index into the document's material array) mesh `index`
    /// uses.
    ///
    /// FIXME The C++ assumes one primitive per mesh; the material really is per
    /// primitive.
    pub fn meshMaterialIndex(self: *const Gltf, index: u32) !u32 {
        const primitive = try self.singlePrimitive(index);
        const gltf_material = primitive.material orelse return error.GltfPrimitiveHasNoMaterial;

        return @intCast((@intFromPtr(gltf_material) - @intFromPtr(self.data.materials)) / @sizeOf(c.cgltf_material));
    }

    fn singlePrimitive(self: *const Gltf, index: u32) !*c.cgltf_primitive {
        const gltf_mesh = self.data.meshes[index];

        // FIXME Assume meshes only contain one primitive.
        if (gltf_mesh.primitives_count != 1) return error.GltfUnsupportedPrimitiveCount;

        const primitive: *c.cgltf_primitive = @ptrCast(gltf_mesh.primitives);

        if (primitive.type != c.cgltf_primitive_type_triangles) return error.GltfUnsupportedPrimitiveType;
        if (primitive.indices == null) return error.GltfPrimitiveHasNoIndices;
        // Need pos, uv, normals, tangents at minimum.
        if (primitive.attributes_count < 4) return error.GltfPrimitiveMissingAttributes;
        if (primitive.has_draco_mesh_compression != 0) return error.GltfDracoUnsupported;

        return primitive;
    }

    pub fn loadMesh(self: *const Gltf, index: u32, allocator: std.mem.Allocator) !mesh_module.Mesh {
        const primitive = try self.singlePrimitive(index);

        var mesh = mesh_module.Mesh{};
        errdefer mesh.deinit(allocator);

        try readAccessor(u32, primitive.indices, allocator, &mesh.indexes);

        var normals: std.ArrayList([3]f32) = .empty;
        defer normals.deinit(allocator);
        var tangents: std.ArrayList([4]f32) = .empty;
        defer tangents.deinit(allocator);
        var uvs: std.ArrayList([2]f32) = .empty;
        defer uvs.deinit(allocator);

        for (primitive.attributes[0..primitive.attributes_count]) |attribute| {
            // FIXME Only the first set of each attribute is read.
            if (attribute.index != 0) continue;

            switch (attribute.type) {
                c.cgltf_attribute_type_position => {
                    try expectType(attribute.data, c.cgltf_type_vec3);
                    try readAccessor([3]f32, attribute.data, allocator, &mesh.positions);
                },
                c.cgltf_attribute_type_normal => {
                    try expectType(attribute.data, c.cgltf_type_vec3);
                    try readAccessor([3]f32, attribute.data, allocator, &normals);
                },
                c.cgltf_attribute_type_tangent => {
                    try expectType(attribute.data, c.cgltf_type_vec4);
                    try readAccessor([4]f32, attribute.data, allocator, &tangents);
                },
                c.cgltf_attribute_type_texcoord => {
                    try expectType(attribute.data, c.cgltf_type_vec2);
                    try readAccessor([2]f32, attribute.data, allocator, &uvs);
                },
                // FIXME Simply ignore the stream.
                else => {},
            }
        }

        // FIXME handle missing streams with defaults
        try mesh.attributes.resize(allocator, mesh.positions.items.len);

        for (mesh.attributes.items, 0..) |*attributes, i| {
            attributes.* = .{
                .normal = normals.items[i],
                .uv = uvs.items[i],
                .tangent = tangents.items[i],
            };
        }

        return mesh;
    }
};

fn expectType(accessor: ?*c.cgltf_accessor, expected: c_uint) !void {
    const a = accessor orelse return error.GltfMissingAccessor;
    if (a.type != expected) return error.GltfUnexpectedAccessorType;
}

/// Reads `accessor.count` elements of `T`, hopping by `accessor.stride` so
/// interleaved buffer views work.
fn readAccessor(
    comptime T: type,
    accessor: ?*c.cgltf_accessor,
    allocator: std.mem.Allocator,
    out: *std.ArrayList(T),
) !void {
    const a = accessor orelse return error.GltfMissingAccessor;
    const buffer_view = a.buffer_view orelse return error.GltfAccessorHasNoBufferView;

    if (a.stride == 0) return error.GltfZeroStride;
    if (a.stride < @sizeOf(T)) return error.GltfStrideTooSmall;

    const buffer = buffer_view.*.buffer orelse return error.GltfBufferViewHasNoBuffer;
    const buffer_data = buffer.*.data orelse return error.GltfBufferNotLoaded;

    const start = @intFromPtr(buffer_data) + buffer_view.*.offset + a.offset;
    const end = @intFromPtr(buffer_data) + buffer.*.size;

    if (a.count > 0 and start + (a.count - 1) * a.stride + @sizeOf(T) > end) return error.GltfAccessorOutOfBounds;

    try out.resize(allocator, a.count);

    for (out.items, 0..) |*element, i| {
        const source: [*]const u8 = @ptrFromInt(start + i * a.stride);
        element.* = std.mem.bytesToValue(T, source[0..@sizeOf(T)]);
    }
}

/// Parses `path`, decodes its buffers and validates it, mirroring the three
/// asserted cgltf calls in GameLoop.cpp.
pub fn load(allocator: std.mem.Allocator, path: []const u8) !Gltf {
    const path_z = try allocator.dupeZ(u8, path);
    defer allocator.free(path_z);

    const options = std.mem.zeroes(c.cgltf_options);
    var data: ?*c.cgltf_data = null;

    if (c.cgltf_parse_file(&options, path_z.ptr, &data) != c.cgltf_result_success) return error.GltfParseFailed;
    errdefer c.cgltf_free(data);

    if (c.cgltf_load_buffers(&options, data, path_z.ptr) != c.cgltf_result_success) return error.GltfLoadBuffersFailed;
    if (c.cgltf_validate(data) != c.cgltf_result_success) return error.GltfValidationFailed;

    const base_path = try allocator.dupe(u8, std.fs.path.dirname(path) orelse ".");
    errdefer allocator.free(base_path);

    const result = Gltf{ .data = data.?, .base_path = base_path, .allocator = allocator };

    log.info("loaded '{s}': {} mesh(es), {} material(s), {} image(s)", .{
        path,
        result.meshCount(),
        result.materialCount(),
        result.imageCount(),
    });

    return result;
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

test "loads the SciFiHelmet glTF" {
    const allocator = std.testing.allocator;

    var gltf = try load(allocator, "res/model/sci_fi_helmet/SciFiHelmet.gltf");
    defer gltf.deinit();

    try std.testing.expectEqual(@as(u32, 1), gltf.meshCount());
    try std.testing.expectEqual(@as(u32, 1), gltf.materialCount());
    try std.testing.expectEqual(@as(u32, 4), gltf.imageCount());

    // Image order drives the texture handle span, so it has to be exact.
    const expected_images = [_][]const u8{
        "res/model/sci_fi_helmet/SciFiHelmet_BaseColor.dds",
        "res/model/sci_fi_helmet/SciFiHelmet_MetallicRoughness.dds",
        "res/model/sci_fi_helmet/SciFiHelmet_Normal.dds",
        "res/model/sci_fi_helmet/SciFiHelmet_AmbientOcclusion.dds",
    };

    for (expected_images, 0..) |expected, i| {
        const path = try gltf.imagePath(@intCast(i), allocator);
        defer allocator.free(path);

        try std.testing.expectEqualStrings(expected, path);
    }

    try std.testing.expectEqual(Material{
        .base_color_image = 0,
        .metal_roughness_image = 1,
        .normal_image = 2,
        .ao_image = 3,
    }, try gltf.material(0));

    try std.testing.expectEqual(@as(u32, 0), try gltf.meshMaterialIndex(0));

    var mesh = try gltf.loadMesh(0, allocator);
    defer mesh.deinit(allocator);

    try std.testing.expectEqual(@as(usize, 70074), mesh.indexCount());
    try std.testing.expectEqual(@as(usize, 70074), mesh.vertexCount());
    try std.testing.expectEqual(mesh.vertexCount(), mesh.attributes.items.len);

    // Spot-check against the accessor bounds declared in the .gltf.
    var min = [3]f32{ std.math.inf(f32), std.math.inf(f32), std.math.inf(f32) };
    var max = [3]f32{ -std.math.inf(f32), -std.math.inf(f32), -std.math.inf(f32) };

    for (mesh.positions.items) |position| {
        for (0..3) |axis| {
            min[axis] = @min(min[axis], position[axis]);
            max[axis] = @max(max[axis], position[axis]);
        }
    }

    try std.testing.expectEqualSlices(f32, &.{ -1.1511525, -1.4587183, -1.2511287 }, &min);
    try std.testing.expectEqualSlices(f32, &.{ 1.1511525, 1.4587184, 1.2511277 }, &max);

    // The tangent w carries the bitangent sign, so it must be ±1 everywhere.
    for (mesh.attributes.items) |attributes| {
        try std.testing.expect(@abs(attributes.tangent[3]) == 1.0);
    }

    for (mesh.indexes.items) |index| {
        try std.testing.expect(index < mesh.vertexCount());
    }
}
