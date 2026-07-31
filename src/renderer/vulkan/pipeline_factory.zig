// Port of src/renderer/vulkan/PipelineFactory.{h,cpp}
//
// Passes register a creator up front and get back a tracker index; the actual
// VkPipeline is built lazily on the first update. The indirection exists so
// pipelines can be rebuilt when shaders change — the version counter is the
// hook for that, even though nothing bumps it yet.
//
// The C++ takes a function pointer; here it is a function pointer too rather
// than a closure, so the registry stays a plain array with no captured state.

const std = @import("std");
const vk = @import("vulkan");

pub const PipelineFunctor = *const fn (
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline;

pub const PipelineCreator = struct {
    pipeline_layout: vk.PipelineLayout,
    pipeline_creation_function: PipelineFunctor,
};

pub const PipelineTracker = struct {
    pub const invalid_index: u32 = 0xFFFF_FFFF;

    creator: PipelineCreator,
    loaded_version: u32 = 0,
    pipeline_index: u32 = invalid_index,
};

pub const PipelineFactory = struct {
    trackers: std.ArrayList(PipelineTracker) = .empty,
    pipelines: std.ArrayList(vk.Pipeline) = .empty,
    dirty: bool = true,

    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) PipelineFactory {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *PipelineFactory, vkd: anytype, device: vk.Device) void {
        for (self.pipelines.items) |pipeline| {
            vkd.destroyPipeline(device, pipeline, null);
        }

        self.pipelines.deinit(self.allocator);
        self.trackers.deinit(self.allocator);
    }

    /// Returns a tracker index.
    pub fn registerPipelineCreator(self: *PipelineFactory, creator: PipelineCreator) !u32 {
        const index: u32 = @intCast(self.trackers.items.len);

        try self.trackers.append(self.allocator, .{ .creator = creator });

        return index;
    }

    /// Builds every pipeline that has not been built yet. Called once per frame
    /// before recording, same as the C++.
    pub fn update(self: *PipelineFactory, vkd: *const vk.DeviceWrapper, device: vk.Device) !void {
        if (!self.dirty) return;

        for (self.trackers.items) |*tracker| {
            if (tracker.loaded_version != 0) continue;

            const pipeline = try tracker.creator.pipeline_creation_function(
                vkd,
                device,
                tracker.creator.pipeline_layout,
            );

            tracker.loaded_version += 1;
            tracker.pipeline_index = @intCast(self.pipelines.items.len);

            try self.pipelines.append(self.allocator, pipeline);
        }

        self.dirty = false;
    }

    pub fn getPipeline(self: *const PipelineFactory, pipeline_tracker_index: u32) vk.Pipeline {
        const tracker = self.trackers.items[pipeline_tracker_index];
        std.debug.assert(tracker.loaded_version != 0);

        return self.pipelines.items[tracker.pipeline_index];
    }
};
