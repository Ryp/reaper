// Port of src/renderer/vulkan/BackendResources.h + BackendResources.cpp
//
// Everything the backend needs that outlives a frame but is not part of the
// device itself. The C++ version creates ~25 sub-resource sets here; they get
// added milestone by milestone, in the same order as the original.

const std = @import("std");
const vk = @import("vulkan");

const CommandBuffer = @import("command_buffer.zig").CommandBuffer;
const debug_gradient = @import("renderpass/debug_gradient.zig");
const frame_sync = @import("frame_sync.zig");
const tone_mapping = @import("renderpass/tone_mapping.zig");
const vma = @import("vma.zig").c;
const FrameGraphResources = @import("framegraph_resources.zig").FrameGraphResources;
const PipelineFactory = @import("pipeline_factory.zig").PipelineFactory;
const SamplerResources = @import("sampler_resources.zig").SamplerResources;
const log = std.log.scoped(.vulkan);

pub const BackendResources = struct {
    gfx_command_pool: vk.CommandPool,
    gfx_cmd_buffer: CommandBuffer,

    frame_sync_resources: frame_sync.FrameSyncResources,
    framegraph_resources: FrameGraphResources,
    pipeline_factory: PipelineFactory,
    sampler_resources: SamplerResources,
    debug_gradient_resources: debug_gradient.Resources,
    tone_map_pass_resources: tone_mapping.ToneMapPassResources,

    /// Reset with .retain_capacity at the top of every frame; all
    /// frame-lifetime allocations come from here.
    frame_arena: std.heap.ArenaAllocator,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        graphics_queue_family_index: u32,
        descriptor_pool: vk.DescriptorPool,
        allocator: std.mem.Allocator,
    ) !BackendResources {
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

        var framegraph_resources = try FrameGraphResources.init(vkd, device, allocator);
        errdefer framegraph_resources.deinit(vkd, device, null);

        var pipeline_factory = PipelineFactory.init(allocator);
        errdefer pipeline_factory.deinit(vkd, device);

        var sampler_resources = try SamplerResources.init(vkd, device);
        errdefer sampler_resources.deinit(vkd, device);

        const debug_gradient_resources = try debug_gradient.Resources.init(vkd, device, descriptor_pool);

        const tone_map_pass_resources = try tone_mapping.ToneMapPassResources.init(
            vkd,
            device,
            descriptor_pool,
            &pipeline_factory,
        );

        return .{
            .gfx_command_pool = gfx_command_pool,
            .gfx_cmd_buffer = .{ .handle = cmd_buffer_handle },
            .frame_sync_resources = frame_sync_resources,
            .framegraph_resources = framegraph_resources,
            .pipeline_factory = pipeline_factory,
            .sampler_resources = sampler_resources,
            .debug_gradient_resources = debug_gradient_resources,
            .tone_map_pass_resources = tone_map_pass_resources,
            .frame_arena = .init(allocator),
        };
    }

    pub fn deinit(self: *BackendResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        self.frame_arena.deinit();

        self.tone_map_pass_resources.deinit(vkd, device);
        self.debug_gradient_resources.deinit(vkd, device);
        self.sampler_resources.deinit(vkd, device);
        self.pipeline_factory.deinit(vkd, device);
        self.framegraph_resources.deinit(vkd, device, vma_instance);

        frame_sync.destroy(vkd, device, self.frame_sync_resources);

        vkd.freeCommandBuffers(device, self.gfx_command_pool, &.{self.gfx_cmd_buffer.handle});
        vkd.destroyCommandPool(device, self.gfx_command_pool, null);
    }
};
