// Port of src/renderer/vulkan/FrameGraphResources.{h,cpp}
//
// Owns the GPU objects behind the frame graph's declared resources. Everything
// except the event pool is volatile: it is torn down and rebuilt whenever the
// graph is rebuilt.

const std = @import("std");
const vk = @import("vulkan");

const buffer_module = @import("buffer.zig");
const fg = @import("../graph/frame_graph.zig");
const image_module = @import("image.zig");
const vma = @import("vma.zig").c;

const gpu_buffer = @import("../buffer/gpu_buffer.zig");
const gpu_texture_properties = @import("../texture/gpu_texture_properties.zig");
const gpu_texture_view = @import("../texture/gpu_texture_view.zig");

pub const FrameGraphTexture = struct {
    properties: gpu_texture_properties.GPUTextureProperties,
    default_view: gpu_texture_view.GPUTextureView,

    handle: vk.Image,
    default_view_handle: vk.ImageView,
    additional_views: []const vk.ImageView,
    image_layout: vk.ImageLayout,
};

pub const FrameGraphBuffer = struct {
    properties: gpu_buffer.GPUBufferProperties,
    default_view: gpu_buffer.GPUBufferView,

    handle: vk.Buffer,
};

pub const FrameGraphResources = struct {
    /// NOTE: you need as many events as concurrent synchronization barriers.
    /// For a first implementation, just have as many events as barriers.
    pub const event_count = 100;

    // Persistent
    events: [event_count]vk.Event,

    // Volatile
    buffers: std.ArrayList(buffer_module.GPUBuffer) = .empty,
    textures: std.ArrayList(image_module.GPUTexture) = .empty,
    default_texture_views: std.ArrayList(vk.ImageView) = .empty,
    additional_texture_views: std.ArrayList(vk.ImageView) = .empty,

    // NOTE: C++ keeps a second copy of the texture arrays purely so they can be
    // swapped at the top of allocate(). The destroy pass runs *before* that
    // swap and only touches the non-_b set, so the swap moves empty arrays
    // around and achieves nothing else. Left out here for that reason.

    allocator: std.mem.Allocator,

    pub fn init(vkd: anytype, device: vk.Device, allocator: std.mem.Allocator) !FrameGraphResources {
        const event_info = vk.EventCreateInfo{
            .s_type = .event_create_info,
            .p_next = null,
            .flags = .{ .device_only_bit = true },
        };

        var events: [event_count]vk.Event = undefined;
        var created: usize = 0;
        errdefer for (events[0..created]) |event| vkd.destroyEvent(device, event, null);

        while (created < event_count) : (created += 1) {
            events[created] = try vkd.createEvent(device, &event_info, null);
        }

        // Volatile stuff is created later
        return .{ .events = events, .allocator = allocator };
    }

    pub fn deinit(self: *FrameGraphResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        self.destroyVolatileResources(vkd, device, vma_instance);

        self.buffers.deinit(self.allocator);
        self.textures.deinit(self.allocator);
        self.default_texture_views.deinit(self.allocator);
        self.additional_texture_views.deinit(self.allocator);

        for (self.events) |event| vkd.destroyEvent(device, event, null);
    }

    pub fn destroyVolatileResources(
        self: *FrameGraphResources,
        vkd: anytype,
        device: vk.Device,
        vma_instance: vma.VmaAllocator,
    ) void {
        for (self.buffers.items) |buffer| {
            if (buffer.handle != .null_handle) buffer_module.destroyBuffer(vma_instance, buffer);
        }
        self.buffers.clearRetainingCapacity();

        for (self.textures.items) |texture| {
            if (texture.handle != .null_handle) image_module.destroyImage(vma_instance, texture);
        }
        self.textures.clearRetainingCapacity();

        for (self.default_texture_views.items) |view| {
            if (view != .null_handle) vkd.destroyImageView(device, view, null);
        }
        self.default_texture_views.clearRetainingCapacity();

        for (self.additional_texture_views.items) |view| {
            if (view != .null_handle) vkd.destroyImageView(device, view, null);
        }
        self.additional_texture_views.clearRetainingCapacity();
    }

    /// FIXME Reuse previous resources here instead of destroying them.
    pub fn allocateVolatileResources(
        self: *FrameGraphResources,
        vkd: anytype,
        device: vk.Device,
        vma_instance: vma.VmaAllocator,
        framegraph: *const fg.FrameGraph,
    ) !void {
        self.destroyVolatileResources(vkd, device, vma_instance);

        const null_buffer = buffer_module.GPUBuffer{
            .handle = .null_handle,
            .allocation = null,
            .properties_deprecated = .{},
        };
        const null_texture = image_module.GPUTexture{ .handle = .null_handle, .allocation = null };

        // ---- Buffers ----
        try self.buffers.appendNTimes(self.allocator, null_buffer, framegraph.buffer_resources.items.len);

        for (framegraph.buffer_resources.items, 0..) |resource, index| {
            if (!resource.is_used) continue;

            self.buffers.items[index] = try buffer_module.createBuffer(
                vma_instance,
                resource.properties.buffer,
                .gpu_only,
            );
        }

        // ---- Textures ----
        try self.textures.appendNTimes(self.allocator, null_texture, framegraph.texture_resources.items.len);
        try self.default_texture_views.appendNTimes(
            self.allocator,
            .null_handle,
            framegraph.texture_resources.items.len,
        );

        for (framegraph.texture_resources.items, 0..) |resource, index| {
            if (!resource.is_used) continue;

            const new_texture = try image_module.createImage(vma_instance, resource.properties.texture);
            self.textures.items[index] = new_texture;

            // NOTE: C++ creates the default view unconditionally, which works
            // there only because every texture it declares happens to be
            // sampled or storage. A transfer-only texture cannot have a view at
            // all, so the usage is checked here.
            if (isViewable(resource.properties.texture.usage_flags)) {
                self.default_texture_views.items[index] = try image_module.createImageView(
                    vkd,
                    device,
                    new_texture.handle,
                    resource.default_view.texture,
                );
            }
        }

        // ---- Additional texture views ----
        try self.additional_texture_views.appendNTimes(
            self.allocator,
            .null_handle,
            framegraph.texture_views.items.len,
        );

        for (framegraph.resource_usages.items) |usage| {
            const view_handles = usage.additional_views;
            if (view_handles.count == 0) continue;

            if (!usage.resource_handle.is_texture or !usage.is_used) continue;

            const texture = self.textures.items[usage.resource_handle.index];
            const views_info = framegraph.texture_views.items[view_handles.offset..][0..view_handles.count];
            const views = self.additional_texture_views.items[view_handles.offset..][0..view_handles.count];

            for (views, views_info) |*view, view_info| {
                view.* = try image_module.createImageView(vkd, device, texture.handle, view_info);
            }
        }
    }

    /// vkCreateImageView rejects an image whose usage has no view-capable bit.
    fn isViewable(usage: gpu_texture_properties.GPUTextureUsage) bool {
        return usage.sampled or usage.storage or usage.color_attachment or
            usage.depth_stencil_attachment or usage.transient_attachment or usage.input_attachment;
    }

    pub fn getTextureHandle(self: *const FrameGraphResources, resource_handle: fg.ResourceHandle) vk.Image {
        std.debug.assert(resource_handle.is_texture);

        const texture = self.textures.items[resource_handle.index];
        std.debug.assert(texture.handle != .null_handle);

        return texture.handle;
    }

    pub fn getTexture(
        self: *const FrameGraphResources,
        framegraph: *const fg.FrameGraph,
        usage_handle: fg.ResourceUsageHandle,
    ) FrameGraphTexture {
        const usage = fg.getResourceUsage(framegraph, usage_handle);
        const resource = fg.getResource(framegraph, usage.resource_handle);
        const view_handles = usage.additional_views;

        return .{
            .properties = resource.properties.texture,
            .default_view = resource.default_view.texture,
            .handle = self.getTextureHandle(usage.resource_handle),
            .default_view_handle = self.default_texture_views.items[usage.resource_handle.index],
            .additional_views = self.additional_texture_views.items[view_handles.offset..][0..view_handles.count],
            .image_layout = usage.access.image_layout,
        };
    }

    pub fn getBufferHandle(self: *const FrameGraphResources, resource_handle: fg.ResourceHandle) vk.Buffer {
        std.debug.assert(!resource_handle.is_texture);

        const buffer = self.buffers.items[resource_handle.index];
        std.debug.assert(buffer.handle != .null_handle);

        return buffer.handle;
    }

    pub fn getBuffer(
        self: *const FrameGraphResources,
        framegraph: *const fg.FrameGraph,
        usage_handle: fg.ResourceUsageHandle,
    ) FrameGraphBuffer {
        const usage = fg.getResourceUsage(framegraph, usage_handle);
        const resource = fg.getResource(framegraph, usage.resource_handle);

        return .{
            .properties = resource.properties.buffer,
            .default_view = resource.default_view.buffer,
            .handle = self.getBufferHandle(usage.resource_handle),
        };
    }
};
