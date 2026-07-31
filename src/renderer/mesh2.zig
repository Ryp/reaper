// Port of src/renderer/Mesh2.h + src/renderer/ResourceHandle.h

pub const MeshHandle = enum(u32) {
    invalid = 0xFFFF_FFFF,
    _,

    pub fn index(self: MeshHandle) u32 {
        return @intFromEnum(self);
    }

    pub fn isValid(self: MeshHandle) bool {
        return self != .invalid;
    }
};

pub const TextureHandle = enum(u32) {
    invalid = 0xFFFF_FFFF,
    _,

    pub fn index(self: TextureHandle) u32 {
        return @intFromEnum(self);
    }

    pub fn isValid(self: TextureHandle) bool {
        return self != .invalid;
    }
};

pub fn HandleSpan(comptime Handle: type) type {
    return struct {
        offset: u32 = 0,
        count: u32 = 0,

        pub const HandleType = Handle;
    };
}

pub const MeshAlloc = struct {
    index_count: u32 = 0,
    vertex_count: u32 = 0,
    meshlet_count: u32 = 0,

    // Buffer offsets
    index_offset: u32 = 0,
    position_offset: u32 = 0,
    attributes_offset: u32 = 0,
    meshlet_offset: u32 = 0,
};

pub const Mesh2 = struct {
    pub const max_mesh_lods = 4;

    lod_count: u32 = 0,
    lods_allocs: [max_mesh_lods]MeshAlloc = @splat(.{}),
};

pub fn createMesh2(alloc: MeshAlloc) Mesh2 {
    var mesh2 = Mesh2{ .lod_count = 1 };
    mesh2.lods_allocs[0] = alloc;
    return mesh2;
}
