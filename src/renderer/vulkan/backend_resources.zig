// Port of src/renderer/vulkan/BackendResources.h + BackendResources.cpp
//
// Everything the backend needs that outlives a frame but is not part of the
// device itself. The C++ version creates ~25 sub-resource sets here; they get
// added milestone by milestone, in the same order as the original.

const std = @import("std");
const vk = @import("vulkan");

const CommandBuffer = @import("command_buffer.zig").CommandBuffer;
const frame_sync = @import("frame_sync.zig");
const log = std.log.scoped(.vulkan);

pub const BackendResources = struct {
    gfx_command_pool: vk.CommandPool,
    gfx_cmd_buffer: CommandBuffer,

    frame_sync_resources: frame_sync.FrameSyncResources,

    /// Reset with .retain_capacity at the top of every frame; all
    /// frame-lifetime allocations come from here.
    frame_arena: std.heap.ArenaAllocator,

    pub fn init(vkd: anytype, device: vk.Device, graphics_queue_family_index: u32, allocator: std.mem.Allocator) !BackendResources {
        const pool_create_info = vk.CommandPoolCreateInfo{
            .s_type = .command_pool_create_info,
            .p_next = null,
            .flags = .{},
            .queue_family_index = graphics_queue_family_index,
        };

        const gfx_command_pool = try vkd.createCommandPool(device, &pool_create_info, null);
        errdefer vkd.destroyCommandPool(device, gfx_command_pool, null);

        log.debug("created command pool", .{});

        const cmd_buffer_alloc_info = vk.CommandBufferAllocateInfo{
            .s_type = .command_buffer_allocate_info,
            .p_next = null,
            .command_pool = gfx_command_pool,
            .level = .primary,
            .command_buffer_count = 1,
        };

        var cmd_buffer_handle: vk.CommandBuffer = .null_handle;
        try vkd.allocateCommandBuffers(device, &cmd_buffer_alloc_info, @ptrCast(&cmd_buffer_handle));

        log.debug("created command buffer", .{});

        const frame_sync_resources = try frame_sync.create(vkd, device);
        errdefer frame_sync.destroy(vkd, device, frame_sync_resources);

        return .{
            .gfx_command_pool = gfx_command_pool,
            .gfx_cmd_buffer = .{ .handle = cmd_buffer_handle },
            .frame_sync_resources = frame_sync_resources,
            .frame_arena = .init(allocator),
        };
    }

    pub fn deinit(self: *BackendResources, vkd: anytype, device: vk.Device) void {
        self.frame_arena.deinit();

        frame_sync.destroy(vkd, device, self.frame_sync_resources);

        vkd.freeCommandBuffers(device, self.gfx_command_pool, &.{self.gfx_cmd_buffer.handle});
        vkd.destroyCommandPool(device, self.gfx_command_pool, null);
    }
};
