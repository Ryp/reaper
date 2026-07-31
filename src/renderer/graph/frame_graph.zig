// Port of src/renderer/graph/FrameGraph.{h,cpp} + FrameGraphBasicTypes.h
//
// The graph is rebuilt from scratch every frame, so everything here is meant to
// be allocated from the frame arena. There are no generational handles for the
// same reason: nothing survives long enough to dangle.
//
// Scheduling is deliberately trivial and matches user record order. It relies
// on render passes being declared in a compatible rendering order.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("../vulkan/barrier.zig");
const gpu_buffer = @import("../buffer/gpu_buffer.zig");
const gpu_texture_properties = @import("../texture/gpu_texture_properties.zig");
const gpu_texture_view = @import("../texture/gpu_texture_view.zig");

const GPUBufferProperties = gpu_buffer.GPUBufferProperties;
const GPUBufferView = gpu_buffer.GPUBufferView;
const GPUTextureProperties = gpu_texture_properties.GPUTextureProperties;
const GPUTextureView = gpu_texture_view.GPUTextureView;

// --------------------------------------------------------------------------
// Handles (FrameGraphBasicTypes.h)
// --------------------------------------------------------------------------

pub const UsageType = packed struct(u32) {
    input: bool = false,
    output: bool = false,
    _reserved: u30 = 0,

    pub const in: UsageType = .{ .input = true };
    pub const out: UsageType = .{ .output = true };
    pub const in_out: UsageType = .{ .input = true, .output = true };

    pub fn eql(a: UsageType, b: UsageType) bool {
        return @as(u32, @bitCast(a)) == @as(u32, @bitCast(b));
    }
};

pub const RenderPassHandle = enum(u32) {
    invalid = 0xFFFF_FFFF,
    _,

    pub fn index(self: RenderPassHandle) u32 {
        return @intFromEnum(self);
    }

    pub fn isValid(self: RenderPassHandle) bool {
        return self != .invalid;
    }
};

pub const ResourceUsageHandle = enum(u32) {
    invalid = 0xFFFF_FFFF,
    _,

    pub fn index(self: ResourceUsageHandle) u32 {
        return @intFromEnum(self);
    }

    pub fn isValid(self: ResourceUsageHandle) bool {
        return self != .invalid;
    }
};

pub const ResourceHandle = packed struct(u32) {
    index: u31,
    is_texture: bool,

    pub const invalid: ResourceHandle = .{ .index = 0x7FFF_FFFF, .is_texture = true };

    pub fn isValid(self: ResourceHandle) bool {
        return self.index != invalid.index;
    }
};

// --------------------------------------------------------------------------
// Graph contents
// --------------------------------------------------------------------------

pub const GPUResourceProperties = union(enum) {
    texture: GPUTextureProperties,
    buffer: GPUBufferProperties,
};

pub const GPUResourceView = union(enum) {
    texture: GPUTextureView,
    buffer: GPUBufferView,
};

pub const GPUResourceAccess = struct {
    stage_mask: vk.PipelineStageFlags2 = .{},
    access_mask: vk.AccessFlags2 = .{},
    image_layout: vk.ImageLayout = .undefined,

    pub fn eql(a: GPUResourceAccess, b: GPUResourceAccess) bool {
        return std.meta.eql(a, b);
    }
};

pub fn toTextureAccess(access: GPUResourceAccess) barrier_module.GPUTextureAccess {
    return .{
        .stage_mask = access.stage_mask,
        .access_mask = access.access_mask,
        .image_layout = access.image_layout,
    };
}

pub fn toBufferAccess(access: GPUResourceAccess) barrier_module.GPUBufferAccess {
    return .{ .stage_mask = access.stage_mask, .access_mask = access.access_mask };
}

pub fn fromTextureAccess(access: barrier_module.GPUTextureAccess) GPUResourceAccess {
    return .{
        .stage_mask = access.stage_mask,
        .access_mask = access.access_mask,
        .image_layout = access.image_layout,
    };
}

pub fn fromBufferAccess(access: barrier_module.GPUBufferAccess) GPUResourceAccess {
    return .{
        .stage_mask = access.stage_mask,
        .access_mask = access.access_mask,
        .image_layout = .undefined,
    };
}

pub const RenderPass = struct {
    debug_name: []const u8,
    has_side_effects: bool,
    resource_usage_handles: std.ArrayList(ResourceUsageHandle) = .empty,
    is_used: bool = false,
};

pub const Resource = struct {
    debug_name: []const u8,
    properties: GPUResourceProperties,
    default_view: GPUResourceView,
    is_used: bool = false,
};

pub const ResourceViewHandles = struct {
    offset: u32 = 0,
    count: u32 = 0,
};

pub const ResourceUsage = struct {
    type: UsageType,
    resource_handle: ResourceHandle,
    render_pass: RenderPassHandle,
    parent_usage_handle: ResourceUsageHandle,
    access: GPUResourceAccess,
    additional_views: ResourceViewHandles,
    is_used: bool = false,
};

pub const FrameGraph = struct {
    render_passes: std.ArrayList(RenderPass) = .empty,
    resource_usages: std.ArrayList(ResourceUsage) = .empty,

    texture_resources: std.ArrayList(Resource) = .empty,
    buffer_resources: std.ArrayList(Resource) = .empty,

    texture_views: std.ArrayList(GPUTextureView) = .empty,
    buffer_views: std.ArrayList(GPUBufferView) = .empty,

    pub fn deinit(self: *FrameGraph, allocator: std.mem.Allocator) void {
        for (self.render_passes.items) |*render_pass| {
            render_pass.resource_usage_handles.deinit(allocator);
        }
        self.render_passes.deinit(allocator);
        self.resource_usages.deinit(allocator);
        self.texture_resources.deinit(allocator);
        self.buffer_resources.deinit(allocator);
        self.texture_views.deinit(allocator);
        self.buffer_views.deinit(allocator);
    }
};

pub fn getResourceUsage(framegraph: *const FrameGraph, handle: ResourceUsageHandle) *const ResourceUsage {
    std.debug.assert(handle.isValid());
    return &framegraph.resource_usages.items[handle.index()];
}

pub fn getResource(framegraph: *const FrameGraph, handle: ResourceHandle) *const Resource {
    std.debug.assert(handle.isValid());

    return if (handle.is_texture)
        &framegraph.texture_resources.items[handle.index]
    else
        &framegraph.buffer_resources.items[handle.index];
}

pub fn getResourceMut(framegraph: *FrameGraph, handle: ResourceHandle) *Resource {
    std.debug.assert(handle.isValid());

    return if (handle.is_texture)
        &framegraph.texture_resources.items[handle.index]
    else
        &framegraph.buffer_resources.items[handle.index];
}

pub fn allocateTextureViews(
    framegraph: *FrameGraph,
    allocator: std.mem.Allocator,
    texture_views: []const GPUTextureView,
) !ResourceViewHandles {
    const view_offset: u32 = @intCast(framegraph.texture_views.items.len);

    try framegraph.texture_views.appendSlice(allocator, texture_views);

    return .{ .offset = view_offset, .count = @intCast(texture_views.len) };
}

pub fn allocateBufferViews(
    framegraph: *FrameGraph,
    allocator: std.mem.Allocator,
    buffer_views: []const GPUBufferView,
) !ResourceViewHandles {
    const view_offset: u32 = @intCast(framegraph.buffer_views.items.len);

    try framegraph.buffer_views.appendSlice(allocator, buffer_views);

    return .{ .offset = view_offset, .count = @intCast(buffer_views.len) };
}

// --------------------------------------------------------------------------
// Directed acyclic graph
// --------------------------------------------------------------------------

pub const DirectedAcyclicGraph = struct {
    pub const Index = u32;

    pub const Node = struct {
        children: std.ArrayList(Index) = .empty,
    };

    nodes: std.ArrayList(Node) = .empty,

    pub fn deinit(self: *DirectedAcyclicGraph, allocator: std.mem.Allocator) void {
        for (self.nodes.items) |*node| node.children.deinit(allocator);
        self.nodes.deinit(allocator);
    }
};

fn hasCyclesRecursive(
    graph: *const DirectedAcyclicGraph,
    node_index: DirectedAcyclicGraph.Index,
    ancestors: *std.ArrayList(DirectedAcyclicGraph.Index),
    allocator: std.mem.Allocator,
) !bool {
    if (std.mem.indexOfScalar(DirectedAcyclicGraph.Index, ancestors.items, node_index) != null) {
        return true;
    }

    try ancestors.append(allocator, node_index);

    for (graph.nodes.items[node_index].children.items) |child_node_index| {
        if (try hasCyclesRecursive(graph, child_node_index, ancestors, allocator)) {
            return true;
        }
    }

    _ = ancestors.pop();
    return false;
}

/// Depth-first traversal.
pub fn hasCycles(
    graph: *const DirectedAcyclicGraph,
    root_nodes: []const DirectedAcyclicGraph.Index,
    allocator: std.mem.Allocator,
) !bool {
    std.debug.assert(root_nodes.len > 0);

    var ancestor_stack: std.ArrayList(DirectedAcyclicGraph.Index) = .empty;
    defer ancestor_stack.deinit(allocator);

    for (root_nodes) |root_node| {
        if (try hasCyclesRecursive(graph, root_node, &ancestor_stack, allocator)) {
            return true;
        }
    }

    return false;
}

/// Breadth-first traversal. `out_closure` must be empty on entry.
pub fn computeTransitiveClosure(
    graph: *const DirectedAcyclicGraph,
    root_nodes: []const DirectedAcyclicGraph.Index,
    out_closure: *std.ArrayList(DirectedAcyclicGraph.Index),
    allocator: std.mem.Allocator,
) !void {
    const node_count = graph.nodes.items.len;

    std.debug.assert(node_count != 0);
    std.debug.assert(node_count < 1000);
    std.debug.assert(root_nodes.len > 0);
    std.debug.assert(out_closure.items.len == 0);

    const visited_nodes = try allocator.alloc(bool, node_count);
    defer allocator.free(visited_nodes);
    @memset(visited_nodes, false);

    try out_closure.appendSlice(allocator, root_nodes);

    // The list grows while it is being walked, which is what makes this
    // breadth-first.
    var closure_index: usize = 0;
    while (closure_index < out_closure.items.len) : (closure_index += 1) {
        const node_index = out_closure.items[closure_index];

        for (graph.nodes.items[node_index].children.items) |dependency_index| {
            std.debug.assert(dependency_index != node_index); // this node self-loops

            if (visited_nodes[dependency_index]) continue;

            try out_closure.append(allocator, dependency_index);
        }

        visited_nodes[node_index] = true;
    }
}

// --------------------------------------------------------------------------
// Scheduling
// --------------------------------------------------------------------------

pub const ResourceUsageEvent = struct {
    render_pass: RenderPassHandle,
    usage_handle: ResourceUsageHandle,
    access: GPUResourceAccess,
};

pub const Barrier = struct {
    src: ResourceUsageEvent,
    dst: ResourceUsageEvent,
};

pub const BarrierType = packed struct(u32) {
    immediate: bool = false,
    split: bool = false,
    execute_before_pass: bool = false,
    execute_after_pass: bool = false,
    _reserved: u28 = 0,

    pub const immediate_after: BarrierType = .{ .immediate = true, .execute_after_pass = true };
    pub const immediate_before: BarrierType = .{ .immediate = true, .execute_before_pass = true };
    pub const split_begin: BarrierType = .{ .split = true, .execute_after_pass = true };
    pub const split_end: BarrierType = .{ .split = true, .execute_before_pass = true };

    pub fn toString(self: BarrierType) []const u8 {
        const bits: u32 = @bitCast(self);
        return switch (bits) {
            @as(u32, @bitCast(immediate_before)) => "ImmediateBefore",
            @as(u32, @bitCast(immediate_after)) => "ImmediateAfter",
            @as(u32, @bitCast(split_begin)) => "SplitBegin",
            @as(u32, @bitCast(split_end)) => "SplitEnd",
            else => unreachable,
        };
    }
};

pub const BarrierEvent = struct {
    barrier_type: BarrierType,
    barrier_handle: u32,
    render_pass_handle: RenderPassHandle,
};

pub const FrameGraphSchedule = struct {
    queue0: std.ArrayList(RenderPassHandle) = .empty,
    barriers: std.ArrayList(Barrier) = .empty,
    barrier_events: std.ArrayList(BarrierEvent) = .empty,

    pub fn deinit(self: *FrameGraphSchedule, allocator: std.mem.Allocator) void {
        self.queue0.deinit(allocator);
        self.barriers.deinit(allocator);
        self.barrier_events.deinit(allocator);
    }
};

fn placeAutomaticBarriers(
    schedule: *FrameGraphSchedule,
    framegraph: *const FrameGraph,
    allocator: std.mem.Allocator,
) !void {
    const texture_count = framegraph.texture_resources.items.len;
    const buffer_count = framegraph.buffer_resources.items.len;

    // One event list per resource; textures first, then buffers, so a single
    // array covers both resource types.
    const per_resource_events = try allocator.alloc(std.ArrayList(ResourceUsageEvent), texture_count + buffer_count);
    defer {
        for (per_resource_events) |*events| events.deinit(allocator);
        allocator.free(per_resource_events);
    }
    @memset(per_resource_events, .empty);

    // Append resource usage by scheduled execution order
    for (schedule.queue0.items) |render_pass_handle| {
        const render_pass = &framegraph.render_passes.items[render_pass_handle.index()];

        for (render_pass.resource_usage_handles.items) |resource_usage_handle| {
            const resource_usage = getResourceUsage(framegraph, resource_usage_handle);
            const resource_handle = resource_usage.resource_handle;

            std.debug.assert(resource_usage.is_used); // accessing unused resource

            const resource_events = &per_resource_events[
                if (resource_handle.is_texture)
                    resource_handle.index
                else
                    resource_handle.index + texture_count
            ];

            // Assume the resource WILL be created every frame and needs to be
            // transitioned out of UNDEFINED layout.
            // FIXME This path is taken for buffers too since I'm too lazy to
            // fix the .back() call just afterwards
            if (resource_events.items.len == 0) {
                try resource_events.append(allocator, .{
                    .render_pass = schedule.queue0.items[0],
                    // FIXME it's wrong but it doesn't break the framegraph (yet)
                    .usage_handle = resource_usage_handle,
                    .access = .{
                        .stage_mask = .{ .top_of_pipe_bit = true },
                        .access_mask = .{},
                        .image_layout = .undefined,
                    },
                });
            }

            const previous_resource_event = &resource_events.items[resource_events.items.len - 1];
            const previous_resource_usage = getResourceUsage(framegraph, previous_resource_event.usage_handle);

            // If we are in a multiple-reader situation, merge both accesses at earliest time
            if (previous_resource_usage.type.eql(.in) and resource_usage.type.eql(.in)) {
                const old_access = previous_resource_usage.access;
                const new_access = resource_usage.access;

                // Using a different image layout is not supported
                std.debug.assert(!resource_handle.is_texture or old_access.image_layout == new_access.image_layout);

                previous_resource_event.access = .{
                    .stage_mask = old_access.stage_mask.merge(new_access.stage_mask),
                    .access_mask = old_access.access_mask.merge(new_access.access_mask),
                    .image_layout = old_access.image_layout,
                };
            } else {
                try resource_events.append(allocator, .{
                    .render_pass = render_pass_handle,
                    .usage_handle = resource_usage_handle,
                    .access = resource_usage.access,
                });
            }
        }
    }

    // Build barriers now that the successive accesses are consolidated
    for (per_resource_events) |resource_events| {
        // A pruned resource still owns a slot here but never gets an event, and
        // Zig's `1..0` range overflows where the C++ loop just does not run.
        if (resource_events.items.len < 2) continue;

        for (1..resource_events.items.len) |i| {
            const src_resource_event = resource_events.items[i - 1];
            const dst_resource_event = resource_events.items[i];

            const usage = getResourceUsage(framegraph, dst_resource_event.usage_handle);

            // Mismatching image layout
            std.debug.assert(!usage.resource_handle.is_texture or
                src_resource_event.access.image_layout != dst_resource_event.access.image_layout);

            try schedule.barriers.append(allocator, .{ .src = src_resource_event, .dst = dst_resource_event });
        }
    }
}

fn barrierEventLessThan(_: void, a: BarrierEvent, b: BarrierEvent) bool {
    if (a.render_pass_handle == b.render_pass_handle) {
        const a_execute_before = a.barrier_type.execute_before_pass;
        const b_execute_after = b.barrier_type.execute_after_pass;

        return a_execute_before and b_execute_after;
    }

    return a.render_pass_handle.index() < b.render_pass_handle.index();
}

/// NOTE: SUPER trivial scheduling for now, matches user record order. We rely
/// on render passes being appended in compatible rendering order. NO fancy
/// multiqueue stuff here. Yet.
pub fn computeSchedule(framegraph: *const FrameGraph, allocator: std.mem.Allocator) !FrameGraphSchedule {
    var schedule = FrameGraphSchedule{};
    errdefer schedule.deinit(allocator);

    for (framegraph.render_passes.items, 0..) |render_pass, render_pass_index| {
        if (render_pass.is_used) {
            try schedule.queue0.append(allocator, @enumFromInt(render_pass_index));
        }
    }

    try placeAutomaticBarriers(&schedule, framegraph, allocator);

    // With the complete list of barriers in hand, build the timeline of
    // commands to execute for each pass. This is where a split barrier turns
    // into two events.
    for (schedule.barriers.items, 0..) |barrier, i| {
        // Test whether the render passes are consecutive; if so an immediate
        // barrier will do.
        // NOTE: This doesn't catch all cases of really consecutive passes
        const is_usage_immediate = (barrier.src.render_pass.index() + 1 == barrier.dst.render_pass.index());

        if (is_usage_immediate) {
            try schedule.barrier_events.append(allocator, .{
                .barrier_type = .immediate_after,
                .barrier_handle = @intCast(i),
                .render_pass_handle = barrier.src.render_pass,
            });
        } else if (barrier.src.render_pass == barrier.dst.render_pass) {
            try schedule.barrier_events.append(allocator, .{
                .barrier_type = .immediate_before,
                .barrier_handle = @intCast(i),
                .render_pass_handle = barrier.dst.render_pass,
            });
        } else {
            try schedule.barrier_events.append(allocator, .{
                .barrier_type = .split_begin,
                .barrier_handle = @intCast(i),
                .render_pass_handle = barrier.src.render_pass,
            });
            try schedule.barrier_events.append(allocator, .{
                .barrier_type = .split_end,
                .barrier_handle = @intCast(i),
                .render_pass_handle = barrier.dst.render_pass,
            });
        }
    }

    // Sort by render pass, then by before/after marker, so that gathering the
    // events for a pass at record time is a contiguous slice.
    //
    // NOTE: C++ uses std::sort, which is not stable. A stable sort is used here
    // so that the barrier order within a pass is reproducible run to run.
    std.sort.block(BarrierEvent, schedule.barrier_events.items, {}, barrierEventLessThan);

    return schedule;
}

/// Assumes the events are sorted by render pass.
pub fn getBarriersToExecute(
    schedule: *const FrameGraphSchedule,
    render_pass_handle: RenderPassHandle,
    execute_before_pass: bool,
) []const BarrierEvent {
    var begin: ?usize = null;
    var end: usize = 0;

    for (schedule.barrier_events.items, 0..) |barrier_event, i| {
        const match_order = execute_before_pass == barrier_event.barrier_type.execute_before_pass;

        if (barrier_event.render_pass_handle == render_pass_handle and match_order) {
            if (begin == null) begin = i;
            end = i + 1;
        }
    }

    if (begin) |b| return schedule.barrier_events.items[b..end];
    return &.{};
}
