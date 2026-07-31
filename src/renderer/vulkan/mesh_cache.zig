// Port of src/renderer/vulkan/MeshCache.{h,cpp}
//
// One big buffer per stream (indices, positions, attributes, meshlets), all
// host-visible, with a bump allocator handing out sub-ranges. Loading a mesh
// runs it through meshoptimizer's clusterizer, reorders the vertex streams into
// meshlet order, and compacts the per-meshlet index runs into one buffer.

const std = @import("std");
const vk = @import("vulkan");

const buffer_module = @import("buffer.zig");
const gpu_buffer = @import("../buffer/gpu_buffer.zig");
const hlsl_meshlet = @import("../hlsl/meshlet/meshlet.zig");
const mesh_module = @import("../../mesh/mesh.zig");
const mesh2 = @import("../mesh2.zig");
const meshlet_builder = @import("../../mesh/meshlet_builder.zig");
const vma = @import("vma.zig").c;

pub const MeshletMesh = meshlet_builder.MeshletMesh;
pub const buildMeshlets = meshlet_builder.buildMeshlets;

const GPUBuffer = buffer_module.GPUBuffer;
const Mesh = mesh_module.Mesh;
const MeshAlloc = mesh2.MeshAlloc;
const MeshHandle = mesh2.MeshHandle;
const Meshlet = hlsl_meshlet.Meshlet;
const VertexAttributes = mesh_module.VertexAttributes;

const log = std.log.scoped(.renderer);

pub const max_index_count: u32 = 2_000_000;
pub const max_vertex_count: u32 = 4_000_000;
pub const max_meshlet_count: u32 = max_vertex_count / 64; // FIXME

pub const MeshCache = struct {
    index_buffer: GPUBuffer,
    vertex_buffer_position: GPUBuffer,
    vertex_attributes_buffer: GPUBuffer,
    meshlet_buffer: GPUBuffer,

    current_index_offset: u32 = 0,
    current_position_offset: u32 = 0,
    current_attributes_offset: u32 = 0,
    current_meshlet_offset: u32 = 0,

    mesh2_instances: std.ArrayList(mesh2.Mesh2) = .empty,

    allocator: std.mem.Allocator,

    pub fn init(vma_instance: vma.VmaAllocator, allocator: std.mem.Allocator) !MeshCache {
        const usage = gpu_buffer.GPUBufferUsage{ .storage_buffer = true };

        const index_buffer = try buffer_module.createBuffer(
            vma_instance,
            gpu_buffer.defaultBufferProperties(max_index_count, @sizeOf(u32), usage),
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, index_buffer);

        const vertex_buffer_position = try buffer_module.createBuffer(
            vma_instance,
            gpu_buffer.defaultBufferProperties(max_vertex_count, 3 * @sizeOf(f32), usage),
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, vertex_buffer_position);

        const vertex_attributes_buffer = try buffer_module.createBuffer(
            vma_instance,
            gpu_buffer.defaultBufferProperties(max_vertex_count, @sizeOf(VertexAttributes), usage),
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, vertex_attributes_buffer);

        const meshlet_buffer = try buffer_module.createBuffer(
            vma_instance,
            gpu_buffer.defaultBufferProperties(max_meshlet_count, @sizeOf(Meshlet), usage),
            .cpu_to_gpu,
        );

        return .{
            .index_buffer = index_buffer,
            .vertex_buffer_position = vertex_buffer_position,
            .vertex_attributes_buffer = vertex_attributes_buffer,
            .meshlet_buffer = meshlet_buffer,
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *MeshCache, vma_instance: vma.VmaAllocator) void {
        self.mesh2_instances.deinit(self.allocator);

        buffer_module.destroyBuffer(vma_instance, self.meshlet_buffer);
        buffer_module.destroyBuffer(vma_instance, self.vertex_attributes_buffer);
        buffer_module.destroyBuffer(vma_instance, self.vertex_buffer_position);
        buffer_module.destroyBuffer(vma_instance, self.index_buffer);
    }

    /// This invalidates all current handles.
    pub fn clearMeshes(self: *MeshCache) void {
        self.mesh2_instances.clearRetainingCapacity();

        self.current_index_offset = 0;
        self.current_position_offset = 0;
        self.current_attributes_offset = 0;
        self.current_meshlet_offset = 0;
    }

    fn allocateMesh(self: *MeshCache, mesh: Mesh, meshlets: []const Meshlet) MeshAlloc {
        const index_count: u32 = @intCast(mesh.indexes.items.len);
        const position_count: u32 = @intCast(mesh.positions.items.len);
        const attributes_count: u32 = @intCast(mesh.attributes.items.len);
        const meshlet_count: u32 = @intCast(meshlets.len);

        std.debug.assert(index_count > 0);
        std.debug.assert(position_count > 0);
        std.debug.assert(position_count == attributes_count);
        std.debug.assert(meshlet_count > 0);

        const alloc = MeshAlloc{
            .index_count = index_count,
            .vertex_count = position_count,
            .meshlet_count = meshlet_count,
            .index_offset = self.current_index_offset,
            .position_offset = self.current_position_offset,
            .attributes_offset = self.current_attributes_offset,
            .meshlet_offset = self.current_meshlet_offset,
        };

        self.current_index_offset += index_count;
        self.current_position_offset += position_count;
        self.current_attributes_offset += position_count;
        self.current_meshlet_offset += meshlet_count;

        std.debug.assert(self.current_index_offset < max_index_count);
        std.debug.assert(self.current_position_offset < max_vertex_count);
        std.debug.assert(self.current_attributes_offset < max_vertex_count);
        std.debug.assert(self.current_meshlet_offset < max_meshlet_count);

        return alloc;
    }
};

/// Mirrors load_meshes(): clusterize, allocate a slot, upload, hand back a
/// handle per input mesh.
pub fn loadMeshes(
    cache: *MeshCache,
    vma_instance: vma.VmaAllocator,
    allocator: std.mem.Allocator,
    meshes: []const Mesh,
    output_handles: []MeshHandle,
) !void {
    std.debug.assert(output_handles.len >= meshes.len);

    for (meshes, 0..) |mesh, mesh_index| {
        var meshlet_mesh = try meshlet_builder.buildMeshlets(allocator, mesh);
        defer meshlet_mesh.deinit(allocator);

        // allocateMesh reads counts off a Mesh, so wrap the flat slices back up.
        var view = Mesh{};
        view.indexes = .{ .items = meshlet_mesh.indexes, .capacity = meshlet_mesh.indexes.len };
        view.positions = .{ .items = meshlet_mesh.positions, .capacity = meshlet_mesh.positions.len };
        view.attributes = .{ .items = meshlet_mesh.attributes, .capacity = meshlet_mesh.attributes.len };

        const alloc = cache.allocateMesh(view, meshlet_mesh.meshlets);

        try uploadMeshToCache(cache, vma_instance, meshlet_mesh, alloc);

        const new_handle: MeshHandle = @enumFromInt(cache.mesh2_instances.items.len);
        try cache.mesh2_instances.append(cache.allocator, mesh2.createMesh2(alloc));

        log.debug("loaded mesh: {} indices, {} vertices, {} meshlets", .{
            alloc.index_count, alloc.vertex_count, alloc.meshlet_count,
        });

        output_handles[mesh_index] = new_handle;
    }
}

fn uploadMeshToCache(
    cache: *MeshCache,
    vma_instance: vma.VmaAllocator,
    meshlet_mesh: MeshletMesh,
    alloc: MeshAlloc,
) !void {
    try buffer_module.uploadBufferData(
        vma_instance,
        cache.index_buffer,
        cache.index_buffer.properties_deprecated,
        std.mem.sliceAsBytes(meshlet_mesh.indexes),
        alloc.index_offset,
    );

    try buffer_module.uploadBufferData(
        vma_instance,
        cache.vertex_buffer_position,
        cache.vertex_buffer_position.properties_deprecated,
        std.mem.sliceAsBytes(meshlet_mesh.positions),
        alloc.position_offset,
    );

    try buffer_module.uploadBufferData(
        vma_instance,
        cache.vertex_attributes_buffer,
        cache.vertex_attributes_buffer.properties_deprecated,
        std.mem.sliceAsBytes(meshlet_mesh.attributes),
        alloc.attributes_offset,
    );

    try buffer_module.uploadBufferData(
        vma_instance,
        cache.meshlet_buffer,
        cache.meshlet_buffer.properties_deprecated,
        std.mem.sliceAsBytes(meshlet_mesh.meshlets),
        alloc.meshlet_offset,
    );
}
