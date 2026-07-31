// Port of src/renderer/vulkan/StorageBufferAllocator.{h,cpp}
//
// A persistently mapped bump allocator for frame-lifetime storage buffer data.
// Reset once per frame in commitToGpu.
//
// The C++ carries "FIXME Alignment is not handled at all at this time / You
// should get a validation error when we need to properly support this". It is
// handled here: every allocation is aligned forward to
// minStorageBufferOffsetAlignment, which is the alignment
// VkDescriptorBufferInfo::offset must satisfy.

const std = @import("std");
const vk = @import("vulkan");

const buffer_module = @import("buffer.zig");
const gpu_buffer = @import("../buffer/gpu_buffer.zig");
const vma = @import("vma.zig").c;

const GPUBufferProperties = gpu_buffer.GPUBufferProperties;

pub const StorageBufferAlloc = struct {
    buffer: vk.Buffer,
    offset_bytes: u64,
    size_bytes: u64,
};

pub const StorageBufferAllocator = struct {
    buffer: buffer_module.GPUBuffer,
    properties: GPUBufferProperties,
    current_offset_bytes: u64,
    mapped_ptr: [*]u8,

    /// The alignment VkDescriptorBufferInfo::offset has to satisfy for a
    /// storage buffer, taken from the device limits.
    offset_alignment: u64,

    pub fn init(
        vma_instance: vma.VmaAllocator,
        size_bytes: u64,
        min_storage_buffer_offset_alignment: u64,
    ) !StorageBufferAllocator {
        // NOTE: We rely on passing 1 byte as the element size to simplify math.
        const properties = gpu_buffer.defaultBufferProperties(size_bytes, 1, .{ .storage_buffer = true });

        const buffer_create_info = vk.BufferCreateInfo{
            .s_type = .buffer_create_info,
            .p_next = null,
            .flags = .{},
            .size = size_bytes,
            .usage = .{ .storage_buffer_bit = true },
            .sharing_mode = .exclusive,
            .queue_family_index_count = 0,
            .p_queue_family_indices = null,
        };

        // NOTE: Vulkan allows only one active mapping per VkMemory object.
        // Since we use a persistent mapping we need to flag it to VMA and get a
        // separate memory object.
        // RANDOM should ideally be SEQUENTIAL but there's no strong guarantee
        // that this happens with our API yet.
        var allocation_create_info = std.mem.zeroes(vma.VmaAllocationCreateInfo);
        allocation_create_info.flags = vma.VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
            vma.VMA_ALLOCATION_CREATE_MAPPED_BIT |
            vma.VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        allocation_create_info.usage = vma.VMA_MEMORY_USAGE_AUTO;

        var buffer: vma.VkBuffer = null;
        var allocation: vma.VmaAllocation = null;
        var allocation_info: vma.VmaAllocationInfo = undefined;

        const result = vma.vmaCreateBuffer(
            vma_instance,
            @ptrCast(&buffer_create_info),
            &allocation_create_info,
            &buffer,
            &allocation,
            &allocation_info,
        );
        if (result != 0) return error.StorageBufferCreationFailed;

        std.debug.assert(min_storage_buffer_offset_alignment > 0);
        std.debug.assert(std.math.isPowerOfTwo(min_storage_buffer_offset_alignment));

        return .{
            .buffer = .{
                .handle = @enumFromInt(@intFromPtr(buffer)),
                .allocation = allocation,
                .properties_deprecated = properties,
            },
            .properties = properties,
            .current_offset_bytes = 0,
            .mapped_ptr = @ptrCast(allocation_info.pMappedData.?),
            .offset_alignment = min_storage_buffer_offset_alignment,
        };
    }

    pub fn deinit(self: *StorageBufferAllocator, vma_instance: vma.VmaAllocator) void {
        buffer_module.destroyBuffer(vma_instance, self.buffer);
    }

    pub fn allocate(self: *StorageBufferAllocator, size_bytes: u64) StorageBufferAlloc {
        // Align forward before handing out the offset: the C++ FIXME says this
        // is unhandled and warns it will trip validation once a case needs it.
        const current_offset = std.mem.alignForward(u64, self.current_offset_bytes, self.offset_alignment);

        self.current_offset_bytes = current_offset + size_bytes;

        // OOM
        std.debug.assert(self.current_offset_bytes <
            self.properties.element_count * self.properties.element_size_bytes);

        return .{
            .buffer = self.buffer.handle,
            .offset_bytes = current_offset,
            .size_bytes = size_bytes,
        };
    }

    pub fn upload(self: *const StorageBufferAllocator, alloc: StorageBufferAlloc, data: []const u8) void {
        // Don't call this function with zero size
        std.debug.assert(alloc.size_bytes > 0);
        std.debug.assert(data.len == alloc.size_bytes);

        @memcpy(self.mapped_ptr[alloc.offset_bytes..][0..data.len], data);
    }

    /// Convenience wrapper: allocate space for `items` and copy them in.
    pub fn allocateAndUpload(self: *StorageBufferAllocator, comptime T: type, items: []const T) StorageBufferAlloc {
        const bytes = std.mem.sliceAsBytes(items);
        const alloc = self.allocate(bytes.len);

        self.upload(alloc, bytes);

        return alloc;
    }

    pub fn commitToGpu(self: *StorageBufferAllocator, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        var allocation_info: vma.VmaAllocationInfo = undefined;
        vma.vmaGetAllocationInfo(vma_instance, self.buffer.allocation, &allocation_info);

        const ranges = [_]vk.MappedMemoryRange{.{
            .s_type = .mapped_memory_range,
            .p_next = null,
            .memory = @enumFromInt(@intFromPtr(allocation_info.deviceMemory)),
            .offset = allocation_info.offset,
            .size = vk.WHOLE_SIZE,
        }};

        vkd.invalidateMappedMemoryRanges(device, &ranges) catch {};

        // Clear allocator offset
        self.current_offset_bytes = 0;
    }
};
