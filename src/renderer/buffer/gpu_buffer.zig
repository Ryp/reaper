// Port of src/renderer/buffer/GPUBufferProperties.h + GPUBufferView.{h,cpp}

pub const GPUBufferUsage = packed struct(u32) {
    transfer_src: bool = false,
    transfer_dst: bool = false,
    uniform_texel_buffer: bool = false,
    storage_texel_buffer: bool = false,
    uniform_buffer: bool = false,
    storage_buffer: bool = false,
    index_buffer: bool = false,
    vertex_buffer: bool = false,
    indirect_buffer: bool = false,
    _reserved: u23 = 0,

    pub const none: GPUBufferUsage = .{};
};

pub const GPUBufferProperties = struct {
    element_count: u64 = 0,
    element_size_bytes: u32 = 0,
    // FIXME the API around the stride parameter is somewhat funky. Maybe
    // there's nicer ways to abstract the concept of discontinuous alignment.
    stride: u32 = 0,
    usage_flags: GPUBufferUsage = .none,
};

pub fn defaultBufferProperties(
    element_count: u64,
    element_size_bytes: u32,
    usage_flags: GPUBufferUsage,
) GPUBufferProperties {
    return .{
        .element_count = element_count,
        .element_size_bytes = element_size_bytes,
        .stride = element_size_bytes,
        .usage_flags = usage_flags,
    };
}

pub const BufferSubresource = struct {
    element_offset: u64,
    element_count: u64,
};

pub const GPUBufferView = struct {
    offset_bytes: u64 = 0,
    size_bytes: u64 = 0,
};

pub fn defaultBufferView(properties: GPUBufferProperties) GPUBufferView {
    return .{
        .offset_bytes = 0,
        .size_bytes = @as(u64, properties.stride) * properties.element_count,
    };
}

pub fn getBufferView(properties: GPUBufferProperties, subresource: BufferSubresource) GPUBufferView {
    return .{
        .offset_bytes = @as(u64, properties.stride) * subresource.element_offset,
        .size_bytes = @as(u64, properties.stride) * subresource.element_count,
    };
}
