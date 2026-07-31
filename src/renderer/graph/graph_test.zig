// Port of src/renderer/test/graph.cpp
//
// The C++ test builds the graph and asserts nothing (its dump call is commented
// out), so it only ever caught crashes. The same scene is declared here, but the
// results are actually checked: which passes survive pruning, the schedule
// order, and the barriers that come out of it.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("../vulkan/barrier.zig");
const builder_module = @import("builder.zig");
const fg = @import("frame_graph.zig");
const gpu_texture_properties = @import("../texture/gpu_texture_properties.zig");

const Builder = builder_module.Builder;
const GPUTextureAccess = barrier_module.GPUTextureAccess;
const ResourceUsageHandle = fg.ResourceUsageHandle;
const defaultTextureProperties = gpu_texture_properties.defaultTextureProperties;

const dummy_usage_flags = gpu_texture_properties.GPUTextureUsage.none;

// The C++ scene passes a zeroed GPUTextureAccess everywhere. Distinct accesses
// are used here so the barriers built from them are actually distinguishable.
const access_write = GPUTextureAccess{
    .stage_mask = .{ .color_attachment_output_bit = true },
    .access_mask = .{ .color_attachment_write_bit = true },
    .image_layout = .attachment_optimal,
};

const access_read = GPUTextureAccess{
    .stage_mask = .{ .fragment_shader_bit = true },
    .access_mask = .{ .shader_sampled_read_bit = true },
    .image_layout = .shader_read_only_optimal,
};

const Scene = struct {
    shadow_rt: ResourceUsageHandle,
    gbuffer_rt: ResourceUsageHandle,
    opaque_rt: ResourceUsageHandle,
    back_buffer_rt: ResourceUsageHandle,
};

fn recordFrame(builder: *Builder) !Scene {
    var scene: Scene = undefined;

    // Shadow
    {
        const shadow_pass = try builder.createRenderPass("Shadow", false);
        const desc = defaultTextureProperties(512, 512, .r16g16b16a16_unorm, dummy_usage_flags);
        scene.shadow_rt = try builder.createTexture(shadow_pass, "VSM", desc, access_write, &.{});
    }

    // GBuffer
    {
        const gbuffer_pass = try builder.createRenderPass("GBuffer", false);
        const desc = defaultTextureProperties(1280, 720, .r16g16b16a16_unorm, dummy_usage_flags);
        scene.gbuffer_rt = try builder.createTexture(gbuffer_pass, "GBuffer", desc, access_write, &.{});
    }

    // PruneMe: reads the gbuffer and writes a texture nobody consumes.
    {
        const useless_pass = try builder.createRenderPass("PruneMe", false);
        _ = try builder.readTexture(useless_pass, scene.gbuffer_rt, access_read, &.{});

        var desc = defaultTextureProperties(4096, 4096, .r16g16_sfloat, dummy_usage_flags);
        desc.sample_count = 4;
        _ = try builder.createTexture(useless_pass, "UselessTexture", desc, access_write, &.{});
    }

    // Lighting
    {
        const lighting_pass = try builder.createRenderPass("Lighting", false);
        _ = try builder.readTexture(lighting_pass, scene.gbuffer_rt, access_read, &.{});
        _ = try builder.readTexture(lighting_pass, scene.shadow_rt, access_read, &.{});

        const desc = defaultTextureProperties(1280, 720, .r16g16b16a16_sfloat, dummy_usage_flags);
        scene.opaque_rt = try builder.createTexture(lighting_pass, "Opaque", desc, access_write, &.{});
    }

    // Composite
    {
        const composite_pass = try builder.createRenderPass("Composite", false);
        _ = try builder.readTexture(composite_pass, scene.opaque_rt, access_read, &.{});

        const desc = defaultTextureProperties(1280, 720, .r8g8b8a8_srgb, dummy_usage_flags);
        scene.back_buffer_rt = try builder.createTexture(composite_pass, "BackBuffer", desc, access_write, &.{});
    }

    // Present — the only pass with side effects, so the only DAG root.
    {
        const present_pass = try builder.createRenderPass("Present", true);
        _ = try builder.readTexture(present_pass, scene.back_buffer_rt, access_read, &.{});
    }

    return scene;
}

fn findPass(framegraph: *const fg.FrameGraph, name: []const u8) *const fg.RenderPass {
    for (framegraph.render_passes.items) |*render_pass| {
        if (std.mem.eql(u8, render_pass.debug_name, name)) return render_pass;
    }
    unreachable;
}

test "frame graph prunes passes nothing depends on" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    var framegraph = fg.FrameGraph{};
    var builder = Builder.init(&framegraph, allocator);

    _ = try recordFrame(&builder);
    try builder.build();

    // Everything on the path to Present survives.
    for ([_][]const u8{ "Shadow", "GBuffer", "Lighting", "Composite", "Present" }) |name| {
        try std.testing.expect(findPass(&framegraph, name).is_used);
    }

    // PruneMe writes a texture nobody reads, so it drops out — that is the
    // whole point of the DAG closure.
    try std.testing.expect(!findPass(&framegraph, "PruneMe").is_used);

    // ...and so does the texture it created.
    for (framegraph.texture_resources.items) |resource| {
        const expected_used = !std.mem.eql(u8, resource.debug_name, "UselessTexture");
        try std.testing.expectEqual(expected_used, resource.is_used);
    }
}

test "schedule keeps declaration order and skips pruned passes" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    var framegraph = fg.FrameGraph{};
    var builder = Builder.init(&framegraph, allocator);

    _ = try recordFrame(&builder);
    try builder.build();

    var schedule = try fg.computeSchedule(&framegraph, allocator);
    defer schedule.deinit(allocator);

    // Scheduling is record order, minus what got pruned.
    const expected = [_][]const u8{ "Shadow", "GBuffer", "Lighting", "Composite", "Present" };
    try std.testing.expectEqual(expected.len, schedule.queue0.items.len);

    for (schedule.queue0.items, expected) |render_pass_handle, expected_name| {
        try std.testing.expectEqualStrings(
            expected_name,
            framegraph.render_passes.items[render_pass_handle.index()].debug_name,
        );
    }
}

test "barriers transition every used texture out of UNDEFINED first" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    var framegraph = fg.FrameGraph{};
    var builder = Builder.init(&framegraph, allocator);

    _ = try recordFrame(&builder);
    try builder.build();

    var schedule = try fg.computeSchedule(&framegraph, allocator);
    defer schedule.deinit(allocator);

    try std.testing.expect(schedule.barriers.items.len > 0);

    var undefined_source_count: usize = 0;
    for (schedule.barriers.items) |barrier| {
        // A barrier that goes nowhere would be a scheduling bug.
        try std.testing.expect(!barrier.src.access.eql(barrier.dst.access));

        if (barrier.src.access.image_layout == .undefined) undefined_source_count += 1;
    }

    // Four textures survive pruning and each needs exactly one initial
    // transition out of UNDEFINED.
    try std.testing.expectEqual(@as(usize, 4), undefined_source_count);
}

test "every barrier is scheduled into exactly one pass slot" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    var framegraph = fg.FrameGraph{};
    var builder = Builder.init(&framegraph, allocator);

    _ = try recordFrame(&builder);
    try builder.build();

    var schedule = try fg.computeSchedule(&framegraph, allocator);
    defer schedule.deinit(allocator);

    // A split barrier produces two events, an immediate one produces one, so
    // every barrier must be referenced at least once and no more than twice.
    const seen = try allocator.alloc(u32, schedule.barriers.items.len);
    @memset(seen, 0);

    for (schedule.barrier_events.items) |event| {
        seen[event.barrier_handle] += 1;
    }

    for (seen) |count| {
        try std.testing.expect(count == 1 or count == 2);
    }

    // Events must be sorted by render pass, with before-events preceding
    // after-events within a pass; recording relies on that.
    var previous_pass: u32 = 0;
    for (schedule.barrier_events.items) |event| {
        try std.testing.expect(event.render_pass_handle.index() >= previous_pass);
        previous_pass = event.render_pass_handle.index();
    }

    // getBarriersToExecute must return contiguous slices that partition the
    // event list.
    var total: usize = 0;
    for (schedule.queue0.items) |render_pass_handle| {
        total += fg.getBarriersToExecute(&schedule, render_pass_handle, true).len;
        total += fg.getBarriersToExecute(&schedule, render_pass_handle, false).len;
    }
    try std.testing.expectEqual(schedule.barrier_events.items.len, total);
}

test "cycle detection" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    var graph = fg.DirectedAcyclicGraph{};
    try graph.nodes.appendNTimes(allocator, .{}, 3);

    // 0 -> 1 -> 2, acyclic
    try graph.nodes.items[0].children.append(allocator, 1);
    try graph.nodes.items[1].children.append(allocator, 2);
    try std.testing.expect(!try fg.hasCycles(&graph, &.{0}, allocator));

    // ...now close the loop.
    try graph.nodes.items[2].children.append(allocator, 0);
    try std.testing.expect(try fg.hasCycles(&graph, &.{0}, allocator));
}

test "transitive closure reaches only what is reachable" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    var graph = fg.DirectedAcyclicGraph{};
    try graph.nodes.appendNTimes(allocator, .{}, 4);

    // 0 -> 1 -> 2, with 3 orphaned.
    try graph.nodes.items[0].children.append(allocator, 1);
    try graph.nodes.items[1].children.append(allocator, 2);

    var closure: std.ArrayList(fg.DirectedAcyclicGraph.Index) = .empty;
    try fg.computeTransitiveClosure(&graph, &.{0}, &closure, allocator);

    try std.testing.expectEqual(@as(usize, 3), closure.items.len);
    try std.testing.expect(std.mem.indexOfScalar(u32, closure.items, 3) == null);
}
