// Port of src/renderer/vulkan/DescriptorSet.{h,cpp}
//
// Vulkan wants a few structures chained by pointer to fill descriptor sets,
// which is tedious to do by hand. DescriptorWriteHelper owns that memory.
//
// The C++ version news up fixed-size arrays and asserts on overflow. Here the
// backing storage is exact-capacity arena slices with appendAssumeCapacity, so
// the pointers written into the VkWriteDescriptorSet cannot be invalidated by a
// later append.

const std = @import("std");
const vk = @import("vulkan");

pub const DescriptorBinding = struct {
    slot: u32,
    count: u32,
    type: vk.DescriptorType,
    stage_mask: vk.ShaderStageFlags,
};

pub fn createDescriptorImageInfo(image_view: vk.ImageView, layout: vk.ImageLayout) vk.DescriptorImageInfo {
    return .{
        .sampler = .null_handle,
        .image_view = image_view,
        .image_layout = layout,
    };
}

pub fn createDescriptorSamplerInfo(sampler: vk.Sampler) vk.DescriptorImageInfo {
    return .{
        .sampler = sampler,
        .image_view = .null_handle,
        .image_layout = .undefined,
    };
}

pub fn createDescriptorBufferInfo(handle: vk.Buffer, offset_bytes: u64, size_bytes: u64) vk.DescriptorBufferInfo {
    return .{
        .buffer = handle,
        .offset = offset_bytes,
        .range = size_bytes,
    };
}

pub fn createImageDescriptorWrite(
    descriptor_set: vk.DescriptorSet,
    binding: u32,
    descriptor_type: vk.DescriptorType,
    image_infos: []const vk.DescriptorImageInfo,
) vk.WriteDescriptorSet {
    return .{
        .s_type = .write_descriptor_set,
        .p_next = null,
        .dst_set = descriptor_set,
        .dst_binding = binding,
        .dst_array_element = 0,
        .descriptor_count = @intCast(image_infos.len),
        .descriptor_type = descriptor_type,
        .p_image_info = image_infos.ptr,
        .p_buffer_info = undefined,
        .p_texel_buffer_view = undefined,
    };
}

pub fn createBufferDescriptorWrite(
    descriptor_set: vk.DescriptorSet,
    binding: u32,
    descriptor_type: vk.DescriptorType,
    buffer_infos: []const vk.DescriptorBufferInfo,
) vk.WriteDescriptorSet {
    return .{
        .s_type = .write_descriptor_set,
        .p_next = null,
        .dst_set = descriptor_set,
        .dst_binding = binding,
        .dst_array_element = 0,
        .descriptor_count = @intCast(buffer_infos.len),
        .descriptor_type = descriptor_type,
        .p_image_info = undefined,
        .p_buffer_info = buffer_infos.ptr,
        .p_texel_buffer_view = undefined,
    };
}

pub fn createTexelBufferViewDescriptorWrite(
    descriptor_set: vk.DescriptorSet,
    binding: u32,
    descriptor_type: vk.DescriptorType,
    texel_buffer_views: []const vk.BufferView,
) vk.WriteDescriptorSet {
    return .{
        .s_type = .write_descriptor_set,
        .p_next = null,
        .dst_set = descriptor_set,
        .dst_binding = binding,
        .dst_array_element = 0,
        .descriptor_count = @intCast(texel_buffer_views.len),
        .descriptor_type = descriptor_type,
        .p_image_info = undefined,
        .p_buffer_info = undefined,
        .p_texel_buffer_view = texel_buffer_views.ptr,
    };
}

pub fn fillLayoutBindings(
    out: []vk.DescriptorSetLayoutBinding,
    descriptor_bindings: []const DescriptorBinding,
) void {
    std.debug.assert(out.len == descriptor_bindings.len);

    for (out, descriptor_bindings) |*layout_binding, binding| {
        layout_binding.* = .{
            .binding = binding.slot,
            .descriptor_type = binding.type,
            .descriptor_count = binding.count,
            .stage_flags = binding.stage_mask,
            .p_immutable_samplers = null,
        };
    }
}

/// Holds the image/buffer infos that VkWriteDescriptorSet points at, so that
/// the writes stay valid until they are flushed.
pub const DescriptorWriteHelper = struct {
    image_infos: []vk.DescriptorImageInfo,
    buffer_infos: []vk.DescriptorBufferInfo,
    texel_buffer_views: []vk.BufferView,

    image_info_count: usize = 0,
    buffer_info_count: usize = 0,
    texel_buffer_view_count: usize = 0,

    writes: std.ArrayList(vk.WriteDescriptorSet) = .empty,

    allocator: std.mem.Allocator,

    /// Capacities are exact: appends never reallocate, so the pointers handed
    /// to Vulkan stay put.
    pub fn init(
        allocator: std.mem.Allocator,
        image_descriptor_count: u32,
        buffer_descriptor_count: u32,
        texel_buffer_descriptor_count: u32,
    ) !DescriptorWriteHelper {
        return .{
            .image_infos = try allocator.alloc(vk.DescriptorImageInfo, image_descriptor_count),
            .buffer_infos = try allocator.alloc(vk.DescriptorBufferInfo, buffer_descriptor_count),
            .texel_buffer_views = try allocator.alloc(vk.BufferView, texel_buffer_descriptor_count),
            .writes = try std.ArrayList(vk.WriteDescriptorSet).initCapacity(
                allocator,
                image_descriptor_count + buffer_descriptor_count + texel_buffer_descriptor_count,
            ),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *DescriptorWriteHelper) void {
        self.writes.deinit(self.allocator);
        self.allocator.free(self.texel_buffer_views);
        self.allocator.free(self.buffer_infos);
        self.allocator.free(self.image_infos);
    }

    fn newImageInfos(self: *DescriptorWriteHelper, count: usize) []vk.DescriptorImageInfo {
        const offset = self.image_info_count;
        std.debug.assert(offset + count <= self.image_infos.len);
        self.image_info_count += count;
        return self.image_infos[offset..][0..count];
    }

    fn newBufferInfo(self: *DescriptorWriteHelper, info: vk.DescriptorBufferInfo) []vk.DescriptorBufferInfo {
        const offset = self.buffer_info_count;
        std.debug.assert(offset + 1 <= self.buffer_infos.len);
        self.buffer_infos[offset] = info;
        self.buffer_info_count += 1;
        return self.buffer_infos[offset..][0..1];
    }

    pub fn appendImage(
        self: *DescriptorWriteHelper,
        descriptor_set: vk.DescriptorSet,
        binding: u32,
        descriptor_type: vk.DescriptorType,
        image_view: vk.ImageView,
        layout: vk.ImageLayout,
    ) void {
        const infos = self.newImageInfos(1);
        infos[0] = createDescriptorImageInfo(image_view, layout);

        self.writes.appendAssumeCapacity(createImageDescriptorWrite(descriptor_set, binding, descriptor_type, infos));
    }

    pub fn appendSampler(
        self: *DescriptorWriteHelper,
        descriptor_set: vk.DescriptorSet,
        binding: u32,
        sampler: vk.Sampler,
    ) void {
        const infos = self.newImageInfos(1);
        infos[0] = createDescriptorSamplerInfo(sampler);

        self.writes.appendAssumeCapacity(createImageDescriptorWrite(descriptor_set, binding, .sampler, infos));
    }

    pub fn appendBuffer(
        self: *DescriptorWriteHelper,
        descriptor_set: vk.DescriptorSet,
        binding: u32,
        descriptor_type: vk.DescriptorType,
        buffer: vk.Buffer,
        offset_bytes: u64,
        size_bytes: u64,
    ) void {
        const infos = self.newBufferInfo(createDescriptorBufferInfo(buffer, offset_bytes, size_bytes));

        self.writes.appendAssumeCapacity(createBufferDescriptorWrite(descriptor_set, binding, descriptor_type, infos));
    }

    pub fn flush(self: *DescriptorWriteHelper, vkd: anytype, device: vk.Device) void {
        if (self.writes.items.len > 0) {
            vkd.updateDescriptorSets(device, self.writes.items, null);
        }

        self.writes.clearRetainingCapacity();
        self.image_info_count = 0;
        self.buffer_info_count = 0;
        self.texel_buffer_view_count = 0;
    }
};
