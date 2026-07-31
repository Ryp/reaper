// Port of src/renderer/vulkan/renderpass/FrameGraphPass.{h,cpp}
//
// C++ wraps this in an RAII FrameGraphBarrierScope whose destructor records the
// after-pass barriers. Zig has no destructors, so the two halves are explicit
// and callers pair them with `defer`.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("../barrier.zig");
const fg = @import("../../graph/frame_graph.zig");
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;

pub const FrameGraphHelper = struct {
    frame_graph: *const fg.FrameGraph,
    schedule: *const fg.FrameGraphSchedule,
    resources: *const FrameGraphResources,
};

pub fn recordBarriers(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: FrameGraphHelper,
    render_pass_handle: fg.RenderPassHandle,
    before: bool,
) void {
    const barrier_events = fg.getBarriersToExecute(helper.schedule, render_pass_handle, before);
    if (barrier_events.len == 0) return;

    for (barrier_events) |barrier_event| {
        const barrier_handle = barrier_event.barrier_handle;
        const barrier = helper.schedule.barriers.items[barrier_handle];

        const dst_usage = fg.getResourceUsage(helper.frame_graph, barrier.dst.usage_handle);
        const resource_handle = dst_usage.resource_handle;
        const resource = fg.getResource(helper.frame_graph, resource_handle);

        // One barrier per event, so the scratch arrays are single-element.
        var image_barriers: [1]vk.ImageMemoryBarrier2 = undefined;
        var buffer_barriers: [1]vk.BufferMemoryBarrier2 = undefined;
        var image_count: usize = 0;
        var buffer_count: usize = 0;

        if (resource_handle.is_texture) {
            image_barriers[0] = barrier_module.getImageBarrierSameQueue(
                helper.resources.getTextureHandle(resource_handle),
                toBarrierSubresource(resource.default_view.texture.subresource),
                fg.toTextureAccess(barrier.src.access),
                fg.toTextureAccess(barrier.dst.access),
            );
            image_count = 1;
        } else {
            buffer_barriers[0] = barrier_module.getBufferBarrierSameQueue(
                helper.resources.getBufferHandle(resource_handle),
                resource.default_view.buffer,
                fg.toBufferAccess(barrier.src.access),
                fg.toBufferAccess(barrier.dst.access),
            );
            buffer_count = 1;
        }

        const dependencies = barrier_module.getDependencyInfo(
            image_barriers[0..image_count],
            buffer_barriers[0..buffer_count],
        );

        if (barrier_event.barrier_type.immediate) {
            vkd.cmdPipelineBarrier2(cmd_buffer, &dependencies);
        } else if (barrier_event.barrier_type.split and barrier_event.barrier_type.execute_after_pass) {
            std.debug.assert(barrier_handle < helper.resources.events.len);
            vkd.cmdSetEvent2(cmd_buffer, helper.resources.events[barrier_handle], &dependencies);
        } else if (barrier_event.barrier_type.split and barrier_event.barrier_type.execute_before_pass) {
            std.debug.assert(barrier_handle < helper.resources.events.len);

            const events = [_]vk.Event{helper.resources.events[barrier_handle]};
            const dependency_infos = [_]vk.DependencyInfo{dependencies};

            vkd.cmdWaitEvents2(cmd_buffer, &events, &dependency_infos);
            vkd.cmdResetEvent2(
                cmd_buffer,
                helper.resources.events[barrier_handle],
                barrier.dst.access.stage_mask.merge(.{ .all_commands_bit = true }), // FIXME
            );
        }
    }
}

fn toBarrierSubresource(
    subresource: @import("../../texture/gpu_texture_view.zig").GPUTextureSubresource,
) barrier_module.GPUTextureSubresource {
    return .{
        .aspect = subresource.aspect.toVk(),
        .mip_offset = subresource.mip_offset,
        .mip_count = subresource.mip_count,
        .layer_offset = subresource.layer_offset,
        .layer_count = subresource.layer_count,
    };
}

/// Records the barriers scheduled before `render_pass_handle`. Pair with
/// `defer endBarrierScope(...)`, which is the RAII scope the C++ uses.
pub fn beginBarrierScope(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: FrameGraphHelper,
    render_pass_handle: fg.RenderPassHandle,
) void {
    recordBarriers(vkd, cmd_buffer, helper, render_pass_handle, true);
}

pub fn endBarrierScope(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: FrameGraphHelper,
    render_pass_handle: fg.RenderPassHandle,
) void {
    recordBarriers(vkd, cmd_buffer, helper, render_pass_handle, false);
}
