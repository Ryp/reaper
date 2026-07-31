// Port of src/renderer/graph/FrameGraphBuilder.{h,cpp}
//
// Retained-mode builder. It borrows a FrameGraph for the declaration phase;
// once build() has run the graph is usable again.

const std = @import("std");

const fg = @import("frame_graph.zig");
const barrier_module = @import("../vulkan/barrier.zig");
const gpu_buffer = @import("../buffer/gpu_buffer.zig");
const gpu_texture_properties = @import("../texture/gpu_texture_properties.zig");
const gpu_texture_view = @import("../texture/gpu_texture_view.zig");

const DirectedAcyclicGraph = fg.DirectedAcyclicGraph;
const FrameGraph = fg.FrameGraph;
const RenderPassHandle = fg.RenderPassHandle;
const ResourceHandle = fg.ResourceHandle;
const ResourceUsageHandle = fg.ResourceUsageHandle;
const ResourceViewHandles = fg.ResourceViewHandles;

pub const Builder = struct {
    graph: *FrameGraph,
    allocator: std.mem.Allocator,

    pub fn init(graph: *FrameGraph, allocator: std.mem.Allocator) Builder {
        return .{ .graph = graph, .allocator = allocator };
    }

    // ----------------------------------------------------------------------
    // Declaration
    // ----------------------------------------------------------------------

    pub fn createRenderPass(self: *Builder, debug_name: []const u8, has_side_effects: bool) !RenderPassHandle {
        // TODO test for name clashes
        try self.graph.render_passes.append(self.allocator, .{
            .debug_name = debug_name,
            .has_side_effects = has_side_effects,
        });

        return @enumFromInt(self.graph.render_passes.items.len - 1);
    }

    pub fn createTexture(
        self: *Builder,
        render_pass_handle: RenderPassHandle,
        name: []const u8,
        texture_properties: gpu_texture_properties.GPUTextureProperties,
        texture_access: barrier_module.GPUTextureAccess,
        additional_texture_views: []const gpu_texture_view.GPUTextureView,
    ) !ResourceUsageHandle {
        const view_handles = try fg.allocateTextureViews(self.graph, self.allocator, additional_texture_views);

        return self.createResourceGeneric(
            render_pass_handle,
            name,
            .{ .texture = texture_properties },
            fg.fromTextureAccess(texture_access),
            true,
            view_handles,
        );
    }

    pub fn createBuffer(
        self: *Builder,
        render_pass_handle: RenderPassHandle,
        name: []const u8,
        buffer_properties: gpu_buffer.GPUBufferProperties,
        buffer_access: barrier_module.GPUBufferAccess,
        additional_buffer_views: []const gpu_buffer.GPUBufferView,
    ) !ResourceUsageHandle {
        const view_handles = try fg.allocateBufferViews(self.graph, self.allocator, additional_buffer_views);

        return self.createResourceGeneric(
            render_pass_handle,
            name,
            .{ .buffer = buffer_properties },
            fg.fromBufferAccess(buffer_access),
            false,
            view_handles,
        );
    }

    pub fn readTexture(
        self: *Builder,
        render_pass_handle: RenderPassHandle,
        input_usage_handle: ResourceUsageHandle,
        texture_access: barrier_module.GPUTextureAccess,
        additional_texture_views: []const gpu_texture_view.GPUTextureView,
    ) !ResourceUsageHandle {
        const view_handles = try fg.allocateTextureViews(self.graph, self.allocator, additional_texture_views);

        return self.usageOfExisting(.in, render_pass_handle, input_usage_handle, fg.fromTextureAccess(texture_access), view_handles);
    }

    pub fn readBuffer(
        self: *Builder,
        render_pass_handle: RenderPassHandle,
        input_usage_handle: ResourceUsageHandle,
        buffer_access: barrier_module.GPUBufferAccess,
        additional_buffer_views: []const gpu_buffer.GPUBufferView,
    ) !ResourceUsageHandle {
        const view_handles = try fg.allocateBufferViews(self.graph, self.allocator, additional_buffer_views);

        return self.usageOfExisting(.in, render_pass_handle, input_usage_handle, fg.fromBufferAccess(buffer_access), view_handles);
    }

    pub fn writeTexture(
        self: *Builder,
        render_pass_handle: RenderPassHandle,
        input_usage_handle: ResourceUsageHandle,
        texture_access: barrier_module.GPUTextureAccess,
        additional_texture_views: []const gpu_texture_view.GPUTextureView,
    ) !ResourceUsageHandle {
        const view_handles = try fg.allocateTextureViews(self.graph, self.allocator, additional_texture_views);

        return self.usageOfExisting(.in_out, render_pass_handle, input_usage_handle, fg.fromTextureAccess(texture_access), view_handles);
    }

    pub fn writeBuffer(
        self: *Builder,
        render_pass_handle: RenderPassHandle,
        input_usage_handle: ResourceUsageHandle,
        buffer_access: barrier_module.GPUBufferAccess,
        additional_buffer_views: []const gpu_buffer.GPUBufferView,
    ) !ResourceUsageHandle {
        const view_handles = try fg.allocateBufferViews(self.graph, self.allocator, additional_buffer_views);

        return self.usageOfExisting(.in_out, render_pass_handle, input_usage_handle, fg.fromBufferAccess(buffer_access), view_handles);
    }

    // ----------------------------------------------------------------------
    // Private
    // ----------------------------------------------------------------------

    fn createResource(
        self: *Builder,
        debug_name: []const u8,
        properties: fg.GPUResourceProperties,
        is_texture: bool,
    ) !ResourceHandle {
        const resource_array = if (is_texture) &self.graph.texture_resources else &self.graph.buffer_resources;

        try resource_array.append(self.allocator, .{
            .debug_name = debug_name,
            .properties = properties,
            .default_view = switch (properties) {
                .texture => |t| .{ .texture = gpu_texture_view.defaultTextureView(t) },
                .buffer => |b| .{ .buffer = gpu_buffer.defaultBufferView(b) },
            },
            .is_used = false, // overridden later
        });

        return .{ .index = @intCast(resource_array.items.len - 1), .is_texture = is_texture };
    }

    fn createResourceUsage(
        self: *Builder,
        usage_type: fg.UsageType,
        render_pass_handle: RenderPassHandle,
        resource_handle: ResourceHandle,
        resource_access: fg.GPUResourceAccess,
        parent_usage_handle: ResourceUsageHandle,
        view_handles: ResourceViewHandles,
    ) !ResourceUsageHandle {
        std.debug.assert(resource_handle.isValid());

        try self.graph.resource_usages.append(self.allocator, .{
            .type = usage_type,
            .resource_handle = resource_handle,
            .render_pass = render_pass_handle,
            .parent_usage_handle = parent_usage_handle,
            .access = resource_access,
            .additional_views = view_handles,
            .is_used = false, // overridden later
        });

        return @enumFromInt(self.graph.resource_usages.items.len - 1);
    }

    fn createResourceGeneric(
        self: *Builder,
        render_pass_handle: RenderPassHandle,
        name: []const u8,
        properties: fg.GPUResourceProperties,
        resource_access: fg.GPUResourceAccess,
        is_texture: bool,
        view_handles: ResourceViewHandles,
    ) !ResourceUsageHandle {
        const resource_handle = try self.createResource(name, properties, is_texture);
        std.debug.assert(resource_handle.isValid());

        const resource_usage_handle = try self.createResourceUsage(
            .out,
            render_pass_handle,
            resource_handle,
            resource_access,
            .invalid,
            view_handles,
        );

        const render_pass = &self.graph.render_passes.items[render_pass_handle.index()];
        try render_pass.resource_usage_handles.append(self.allocator, resource_usage_handle);

        return resource_usage_handle;
    }

    /// Shared by read_* and write_*: both declare a usage of an already
    /// existing resource, differing only in the usage type.
    fn usageOfExisting(
        self: *Builder,
        usage_type: fg.UsageType,
        render_pass_handle: RenderPassHandle,
        input_usage_handle: ResourceUsageHandle,
        resource_access: fg.GPUResourceAccess,
        view_handles: ResourceViewHandles,
    ) !ResourceUsageHandle {
        const resource_handle = fg.getResourceUsage(self.graph, input_usage_handle).resource_handle;
        std.debug.assert(resource_handle.isValid());

        const resource_usage_handle = try self.createResourceUsage(
            usage_type,
            render_pass_handle,
            resource_handle,
            resource_access,
            input_usage_handle,
            view_handles,
        );

        const render_pass = &self.graph.render_passes.items[render_pass_handle.index()];
        try render_pass.resource_usage_handles.append(self.allocator, resource_usage_handle);

        return resource_usage_handle;
    }

    // ----------------------------------------------------------------------
    // Build
    // ----------------------------------------------------------------------

    pub fn build(self: *Builder) !void {
        var root_nodes: std.ArrayList(DirectedAcyclicGraph.Index) = .empty;
        defer root_nodes.deinit(self.allocator);

        var dag = try self.convertFrameGraphToDag(&root_nodes);
        defer dag.deinit(self.allocator);

        std.debug.assert(!try fg.hasCycles(&dag, root_nodes.items, self.allocator));

        // Compute the set of useful nodes with a flood fill from root nodes
        var closure: std.ArrayList(DirectedAcyclicGraph.Index) = .empty;
        defer closure.deinit(self.allocator);

        try fg.computeTransitiveClosure(&dag, root_nodes.items, &closure, self.allocator);

        self.fillFrameGraphUsedNodes(closure.items);
    }

    /// Builds an alternate representation of the graph that is easier to
    /// process.
    ///
    /// Resource usage node indices are the same in both graphs and can be used
    /// interchangeably. Render pass node indices are NOT, and have to be offset
    /// by the resource usage count.
    ///
    /// Edges are directed in REVERSE compared to the rendering flow. That is
    /// the critical, non-obvious part. Draw a graph.
    fn convertFrameGraphToDag(
        self: *Builder,
        out_root_nodes: *std.ArrayList(DirectedAcyclicGraph.Index),
    ) !DirectedAcyclicGraph {
        const resource_usage_count: u32 = @intCast(self.graph.resource_usages.items.len);
        const render_pass_count: u32 = @intCast(self.graph.render_passes.items.len);

        var dag = DirectedAcyclicGraph{};
        errdefer dag.deinit(self.allocator);

        try dag.nodes.appendNTimes(self.allocator, .{}, resource_usage_count + render_pass_count);

        for (self.graph.render_passes.items, 0..) |render_pass, render_pass_index| {
            const render_pass_index_in_dag: DirectedAcyclicGraph.Index =
                @intCast(render_pass_index + resource_usage_count);

            // Render passes with side effects cannot be pruned. They are the
            // terminal nodes of the DAG, which is what lets orphan nodes be
            // pruned later.
            if (render_pass.has_side_effects) {
                try out_root_nodes.append(self.allocator, render_pass_index_in_dag);
            }

            for (render_pass.resource_usage_handles.items) |resource_usage_handle| {
                const resource_usage_index_in_dag: DirectedAcyclicGraph.Index = resource_usage_handle.index();
                const resource_usage = fg.getResourceUsage(self.graph, resource_usage_handle);

                if (resource_usage.type.eql(.in)) {
                    try dag.nodes.items[render_pass_index_in_dag].children
                        .append(self.allocator, resource_usage_index_in_dag);

                    // Extra edge linking to the previously written/created resource
                    std.debug.assert(resource_usage.parent_usage_handle.isValid());

                    try dag.nodes.items[resource_usage_index_in_dag].children
                        .append(self.allocator, resource_usage.parent_usage_handle.index());
                } else if (resource_usage.type.eql(.out)) {
                    try dag.nodes.items[resource_usage_index_in_dag].children
                        .append(self.allocator, render_pass_index_in_dag);
                } else if (resource_usage.type.eql(.in_out)) {
                    try dag.nodes.items[resource_usage_index_in_dag].children
                        .append(self.allocator, render_pass_index_in_dag);

                    std.debug.assert(resource_usage.parent_usage_handle.isValid());

                    try dag.nodes.items[resource_usage_index_in_dag].children
                        .append(self.allocator, resource_usage.parent_usage_handle.index());
                } else {
                    unreachable;
                }
            }
        }

        // Check if the whole graph is likely to be pruned
        std.debug.assert(out_root_nodes.items.len > 0); // no render pass has the has_side_effects flag

        return dag;
    }

    /// Recovers the is_used flags from the DAG closure.
    ///
    /// Be careful with node indices: DAG indices are NOT FrameGraph indices.
    fn fillFrameGraphUsedNodes(self: *Builder, used_nodes: []const DirectedAcyclicGraph.Index) void {
        for (self.graph.resource_usages.items) |*resource_usage| resource_usage.is_used = false;
        for (self.graph.texture_resources.items) |*resource| resource.is_used = false;
        for (self.graph.buffer_resources.items) |*resource| resource.is_used = false;
        for (self.graph.render_passes.items) |*render_pass| render_pass.is_used = false;

        const resource_count: u32 = @intCast(self.graph.resource_usages.items.len);

        for (used_nodes) |node_index_in_dag| {
            if (node_index_in_dag < resource_count) {
                self.graph.resource_usages.items[node_index_in_dag].is_used = true;
            } else {
                self.graph.render_passes.items[node_index_in_dag - resource_count].is_used = true;
            }
        }

        for (self.graph.render_passes.items) |render_pass| {
            if (!render_pass.is_used) continue;

            for (render_pass.resource_usage_handles.items) |resource_usage_handle| {
                const resource_usage = &self.graph.resource_usages.items[resource_usage_handle.index()];

                if (resource_usage.type.output) {
                    resource_usage.is_used = true;
                }
            }
        }

        for (self.graph.resource_usages.items) |resource_usage| {
            if (!resource_usage.is_used) continue;

            fg.getResourceMut(self.graph, resource_usage.resource_handle).is_used = true;
        }
    }
};
