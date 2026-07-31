// Port of src/renderer/vulkan/Buffer.{h,cpp}

const std = @import("std");
const vk = @import("vulkan");

const vma = @import("vma.zig").c;
const gpu_buffer = @import("../buffer/gpu_buffer.zig");

const GPUBufferProperties = gpu_buffer.GPUBufferProperties;

pub const GPUBuffer = struct {
    handle: vk.Buffer,
    allocation: vma.VmaAllocation,
    properties_deprecated: GPUBufferProperties, // FIXME
};

pub const MemUsage = enum {
    gpu_only,
    cpu_to_gpu,
    gpu_to_cpu,
    cpu_only, // FIXME
};

pub fn bufferUsageToVulkan(usage_flags: gpu_buffer.GPUBufferUsage) vk.BufferUsageFlags {
    return .{
        .transfer_src_bit = usage_flags.transfer_src,
        .transfer_dst_bit = usage_flags.transfer_dst,
        .uniform_texel_buffer_bit = usage_flags.uniform_texel_buffer,
        .storage_texel_buffer_bit = usage_flags.storage_texel_buffer,
        .uniform_buffer_bit = usage_flags.uniform_buffer,
        .storage_buffer_bit = usage_flags.storage_buffer,
        .index_buffer_bit = usage_flags.index_buffer,
        .vertex_buffer_bit = usage_flags.vertex_buffer,
        .indirect_buffer_bit = usage_flags.indirect_buffer,
    };
}

pub fn createBuffer(
    allocator: vma.VmaAllocator,
    input_properties: GPUBufferProperties,
    mem_usage: MemUsage,
) !GPUBuffer {
    var properties = input_properties;

    // Uniform buffers need extra care on the CPU side: there is a minimum
    // buffer offset alignment, which means there can be padding between
    // elements no matter what the element size is.
    if (properties.usage_flags.uniform_buffer) {
        const min_uniform_buffer_offset_alignment: u32 = 0x40; // FIXME
        properties.stride = @max(properties.element_size_bytes, min_uniform_buffer_offset_alignment);
    } else {
        properties.stride = properties.element_size_bytes;
    }

    const buffer_info = vk.BufferCreateInfo{
        .s_type = .buffer_create_info,
        .p_next = null,
        .flags = .{},
        .size = properties.element_count * properties.stride,
        .usage = bufferUsageToVulkan(properties.usage_flags),
        .sharing_mode = .exclusive,
        .queue_family_index_count = 0,
        .p_queue_family_indices = null,
    };

    var alloc_info = std.mem.zeroes(vma.VmaAllocationCreateInfo);
    alloc_info.usage = switch (mem_usage) {
        .gpu_only => vma.VMA_MEMORY_USAGE_GPU_ONLY,
        .cpu_to_gpu => vma.VMA_MEMORY_USAGE_CPU_TO_GPU,
        .gpu_to_cpu => vma.VMA_MEMORY_USAGE_GPU_TO_CPU,
        .cpu_only => vma.VMA_MEMORY_USAGE_CPU_ONLY,
    };

    var buffer: vma.VkBuffer = null;
    var allocation: vma.VmaAllocation = null;

    const result = vma.vmaCreateBuffer(
        allocator,
        @ptrCast(&buffer_info),
        &alloc_info,
        &buffer,
        &allocation,
        null,
    );
    if (result != 0) return error.BufferCreationFailed;

    return .{
        .handle = @enumFromInt(@intFromPtr(buffer)),
        .allocation = allocation,
        .properties_deprecated = properties,
    };
}

pub fn destroyBuffer(allocator: vma.VmaAllocator, buffer: GPUBuffer) void {
    vma.vmaDestroyBuffer(allocator, @ptrFromInt(@intFromEnum(buffer.handle)), buffer.allocation);
}

/// Copies `data` into the buffer, honouring the stride when it differs from
/// the element size.
pub fn uploadBufferData(
    allocator: vma.VmaAllocator,
    buffer: GPUBuffer,
    properties: GPUBufferProperties,
    data: []const u8,
    offset_elements: u32,
) !void {
    var mapped: ?*anyopaque = null;
    if (vma.vmaMapMemory(allocator, buffer.allocation, &mapped) != 0) {
        return error.BufferMapFailed;
    }
    defer vma.vmaUnmapMemory(allocator, buffer.allocation);

    const write_ptr: [*]u8 = @ptrCast(mapped.?);
    const stride = properties.stride;

    std.debug.assert(data.len % properties.element_size_bytes == 0);
    const element_count = data.len / properties.element_size_bytes;

    if (stride == properties.element_size_bytes) {
        const offset_bytes = @as(usize, offset_elements) * stride;
        @memcpy(write_ptr[offset_bytes..][0..data.len], data);
    } else {
        // Strided copy, element by element.
        for (0..element_count) |i| {
            const src = data[i * properties.element_size_bytes ..][0..properties.element_size_bytes];
            const dst_offset = (offset_elements + i) * stride;
            @memcpy(write_ptr[dst_offset..][0..properties.element_size_bytes], src);
        }
    }
}
