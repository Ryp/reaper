// Port of the frame driver in src/renderer/vulkan/renderpass/TestGraphics.cpp
// (resize_swapchain() and backend_execute_frame()).
//
// M0 records a bare vkCmdClearColorImage instead of the real frame graph; the
// surrounding sync — timeline wait, acquire with retries, submit2, present —
// is already the final shape.

const std = @import("std");
const vk = @import("vulkan");
const log = std.log.scoped(.vulkan);

const barrier = @import("barrier.zig");
const debug_gradient = @import("renderpass/debug_gradient.zig");
const descriptor_set = @import("descriptor_set.zig");
const fg = @import("../graph/frame_graph.zig");
const frame_graph_pass = @import("renderpass/frame_graph_pass.zig");
const screenshot = @import("screenshot.zig");
const tone_mapping = @import("renderpass/tone_mapping.zig");
const Builder = @import("../graph/builder.zig").Builder;
const Swapchain = @import("Swapchain.zig");
const BackendResources = @import("backend_resources.zig").BackendResources;
const VulkanBackend = @import("Backend.zig").VulkanBackend;

/// vkAcquireNextImageKHR can legitimately answer NOT_READY; the C++ code
/// spins on it a bounded number of times before giving up on the frame.
const max_acquire_try_count = 10;

const acquire_timeout_ns: u64 = 1_000_000_000;
const timeline_wait_timeout_ns: u64 = 1_000_000_000;

// The swapchain image accesses from TestGraphics.cpp:353-363.
const swapchain_access_initial = barrier.GPUTextureAccess{
    .stage_mask = .{ .bottom_of_pipe_bit = true },
    .access_mask = .{},
    .image_layout = .undefined,
};

const swapchain_access_present = barrier.GPUTextureAccess{
    .stage_mask = .{ .bottom_of_pipe_bit = true },
    .access_mask = .{},
    .image_layout = .present_src_khr,
};

// The frame graph's last pass blits into the swapchain image, so this is the
// layout the swapchain has to be in while the graph runs. It goes back to
// ATTACHMENT_OPTIMAL once the real swapchain pass renders into it.
const swapchain_access_render = barrier.GPUTextureAccess{
    .stage_mask = .{ .blit_bit = true },
    .access_mask = .{ .transfer_write_bit = true },
    .image_layout = .transfer_dst_optimal,
};

/// Mirrors resize_swapchain(). This is the consumer of new_swapchain_extent —
/// the old Zig port never called it, so the window could not be resized.
pub fn resizeSwapchain(backend: *VulkanBackend) !void {
    if (backend.new_swapchain_extent.width == 0 and backend.new_swapchain_extent.height == 0) {
        return;
    }

    // FIXME Same coarse sync as the C++ side.
    try backend.vkd.queueWaitIdle(backend.present_queue);

    std.debug.assert(backend.new_swapchain_extent.width > 0);
    std.debug.assert(backend.new_swapchain_extent.height > 0);

    try Swapchain.resizeVulkanWmSwapchain(
        backend.vki,
        backend.vkd,
        backend.device,
        backend.physical_device.handle,
        &backend.present_info,
        backend.new_swapchain_extent,
        backend.allocator,
    );

    backend.updateRenderExtent();

    backend.new_swapchain_extent = .{ .width = 0, .height = 0 };
}

/// Mirrors backend_execute_frame(). Returns the swapchain image index that was
/// presented, or null when the frame was dropped because the swapchain went out
/// of date — the caller just runs another iteration.
pub fn executeFrame(
    backend: *VulkanBackend,
    resources: *BackendResources,
    readback: ?*const screenshot.Readback,
) !?u32 {
    const device = backend.device;
    const vkd = backend.vkd;

    // Mirrors backend_execute_frame(): the factory builds any pipeline that
    // has not been created yet before anything is recorded.
    try resources.pipeline_factory.update(&vkd, device);

    // ---- Wait for the GPU to be done with frame N-1 ----
    {
        const frame_index_to_wait = backend.frame_index;

        const timeline_semaphore_wait_info = vk.SemaphoreWaitInfo{
            .s_type = .semaphore_wait_info,
            .p_next = null,
            .flags = .{},
            .semaphore_count = 1,
            .p_semaphores = @ptrCast(&resources.frame_sync_resources.timeline_semaphore),
            .p_values = @ptrCast(&frame_index_to_wait),
        };

        while (true) {
            const wait_result = try vkd.waitSemaphores(device, &timeline_semaphore_wait_info, timeline_wait_timeout_ns);
            if (wait_result != .timeout) break;
            log.debug("timeline semaphore wait timed out, retrying", .{});
        }
    }

    backend.frame_index += 1;

    _ = resources.frame_arena.reset(.retain_capacity);

    // ---- Acquire ----
    var acquire = vk.DeviceWrapper.AcquireNextImageKHRResult{ .result = .not_ready, .image_index = 0 };

    for (0..max_acquire_try_count) |_| {
        acquire = vkd.acquireNextImageKHR(
            device,
            backend.present_info.swapchain,
            acquire_timeout_ns,
            backend.semaphore_swapchain_image_available,
            .null_handle,
        ) catch |err| switch (err) {
            // The window can change state between event handling and acquire;
            // drop the frame and let the resize path rebuild the swapchain.
            error.OutOfDateKHR => {
                log.warn("acquire out of date", .{});
                backend.new_swapchain_extent = backend.present_info.surface_extent;
                return null;
            },
            else => return err,
        };

        if (acquire.result != .not_ready) break;
    }

    switch (acquire.result) {
        .success => {},
        .not_ready => log.debug("acquire still not ready after {} tries", .{max_acquire_try_count}),
        .suboptimal_khr => log.debug("acquire suboptimal", .{}),
        .timeout => {
            log.warn("acquire timed out", .{});
            return null;
        },
        else => unreachable,
    }

    const current_swapchain_index = acquire.image_index;

    // ---- Record ----
    const cmd_buffer = resources.gfx_cmd_buffer.handle;

    try vkd.resetCommandPool(device, resources.gfx_command_pool, .{});

    const cmd_buffer_begin_info = vk.CommandBufferBeginInfo{
        .s_type = .command_buffer_begin_info,
        .p_next = null,
        .flags = .{ .one_time_submit_bit = true },
        .p_inheritance_info = null,
    };

    try vkd.beginCommandBuffer(cmd_buffer, &cmd_buffer_begin_info);

    const subresource = barrier.defaultTextureSubresourceOneColorMip();
    const swapchain_image = backend.present_info.images[current_swapchain_index];

    // ---- Declare the frame graph ----
    //
    // Rebuilt from the frame arena every frame, matching the C++ shape:
    // declare, build, allocate volatile resources, update descriptors,
    // schedule, then record.
    const frame_allocator = resources.frame_arena.allocator();

    var framegraph = fg.FrameGraph{};
    var builder = Builder.init(&framegraph, frame_allocator);

    // The LUT bake needs no scene, so it can run alongside the gradient demo
    // and exercise the pipeline factory before any real pass exists. It has to
    // be marked side-effecting until the swapchain pass consumes the LUT,
    // otherwise the graph correctly prunes it as unreachable.
    const tone_map_record = try tone_mapping.createFrameGraphRecord(&builder, true);

    const gradient_record = try debug_gradient.createFrameGraphRecord(
        &builder,
        backend.present_info.surface_extent,
        .r8g8b8a8_unorm,
    );

    try builder.build();

    try resources.framegraph_resources.allocateVolatileResources(
        vkd,
        device,
        backend.vma_instance,
        &framegraph,
    );

    {
        var write_helper = try descriptor_set.DescriptorWriteHelper.init(frame_allocator, 8, 8, 1);
        defer write_helper.deinit();

        tone_mapping.updateDescriptorSet(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            tone_map_record,
            &resources.tone_map_pass_resources,
        );

        debug_gradient.updateDescriptorSet(
            &write_helper,
            &resources.debug_gradient_resources,
            &resources.framegraph_resources,
            &framegraph,
            gradient_record,
        );

        write_helper.flush(vkd, device);
    }

    var schedule = try fg.computeSchedule(&framegraph, frame_allocator);
    defer schedule.deinit(frame_allocator);

    // ---- Record ----

    // An image straight out of a freshly created swapchain is in UNDEFINED
    // layout; every later frame gets it back in PRESENT_SRC.
    const pending_initial_transition = &backend.present_info.images_pending_initial_transition[current_swapchain_index];
    const src_access = if (pending_initial_transition.*) swapchain_access_initial else swapchain_access_present;
    pending_initial_transition.* = false;

    {
        const image_barriers = [_]vk.ImageMemoryBarrier2{
            barrier.getImageBarrierSameQueue(swapchain_image, subresource, src_access, swapchain_access_render),
        };

        const dependencies = barrier.getImageBarrierDependencyInfo(&image_barriers);
        vkd.cmdPipelineBarrier2(cmd_buffer, &dependencies);
    }

    const frame_graph_helper = frame_graph_pass.FrameGraphHelper{
        .frame_graph = &framegraph,
        .schedule = &schedule,
        .resources = &resources.framegraph_resources,
    };

    tone_mapping.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        tone_map_record,
        &resources.tone_map_pass_resources,
        backend.present_info.tonemap_min_nits,
        backend.present_info.tonemap_max_nits,
    );

    debug_gradient.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.debug_gradient_resources,
        gradient_record,
        swapchain_image,
    );

    // Back to PRESENT_SRC before handing the image to the presentation engine.
    if (readback) |readback_target| {
        // The readback has to happen here, while the frame still owns the
        // image: transitioning an image that has already been presented is
        // invalid.
        const to_copy = [_]vk.ImageMemoryBarrier2{
            barrier.getImageBarrierSameQueue(swapchain_image, subresource, swapchain_access_render, screenshot.access_copy),
        };
        vkd.cmdPipelineBarrier2(cmd_buffer, &barrier.getImageBarrierDependencyInfo(&to_copy));

        screenshot.recordCopy(vkd, cmd_buffer, swapchain_image, readback_target);
    }

    {
        const src_layout = if (readback != null) screenshot.access_copy else swapchain_access_render;

        const image_barriers = [_]vk.ImageMemoryBarrier2{
            barrier.getImageBarrierSameQueue(swapchain_image, subresource, src_layout, swapchain_access_present),
        };

        const dependencies = barrier.getImageBarrierDependencyInfo(&image_barriers);
        vkd.cmdPipelineBarrier2(cmd_buffer, &dependencies);
    }

    try vkd.endCommandBuffer(cmd_buffer);

    // ---- Submit ----
    const wait_semaphore_info = vk.SemaphoreSubmitInfo{
        .s_type = .semaphore_submit_info,
        .p_next = null,
        .semaphore = backend.semaphore_swapchain_image_available,
        .value = 0, // NOTE: only meaningful for timeline semaphores
        .stage_mask = swapchain_access_present.stage_mask,
        .device_index = 0,
    };

    const command_buffer_info = vk.CommandBufferSubmitInfo{
        .s_type = .command_buffer_submit_info,
        .p_next = null,
        .command_buffer = cmd_buffer,
        .device_mask = 0,
    };

    const signal_semaphore_info = [_]vk.SemaphoreSubmitInfo{
        .{
            .s_type = .semaphore_submit_info,
            .p_next = null,
            .semaphore = backend.present_info.semaphores_rendering_finished[current_swapchain_index],
            .value = 0,
            .stage_mask = .{ .all_commands_bit = true },
            .device_index = 0,
        },
        .{
            .s_type = .semaphore_submit_info,
            .p_next = null,
            .semaphore = resources.frame_sync_resources.timeline_semaphore,
            .value = backend.frame_index,
            .stage_mask = .{ .all_commands_bit = true },
            .device_index = 0,
        },
    };

    const submit_info_2 = [_]vk.SubmitInfo2{.{
        .s_type = .submit_info_2,
        .p_next = null,
        .flags = .{},
        .wait_semaphore_info_count = 1,
        .p_wait_semaphore_infos = @ptrCast(&wait_semaphore_info),
        .command_buffer_info_count = 1,
        .p_command_buffer_infos = @ptrCast(&command_buffer_info),
        .signal_semaphore_info_count = signal_semaphore_info.len,
        .p_signal_semaphore_infos = &signal_semaphore_info,
    }};

    try vkd.queueSubmit2(backend.graphics_queue, &submit_info_2, .null_handle);

    // ---- Present ----
    const present_info = vk.PresentInfoKHR{
        .s_type = .present_info_khr,
        .p_next = null,
        .wait_semaphore_count = 1,
        .p_wait_semaphores = @ptrCast(&backend.present_info.semaphores_rendering_finished[current_swapchain_index]),
        .swapchain_count = 1,
        .p_swapchains = @ptrCast(&backend.present_info.swapchain),
        .p_image_indices = @ptrCast(&current_swapchain_index),
        .p_results = null,
    };

    const present_result = vkd.queuePresentKHR(backend.present_queue, &present_info) catch |err| switch (err) {
        // The window can change state between event handling and presenting,
        // so this is expected as long as we rebuild a correct swapchain later.
        error.OutOfDateKHR => {
            log.warn("present out of date", .{});
            backend.new_swapchain_extent = backend.present_info.surface_extent;
            return null;
        },
        else => return err,
    };

    if (present_result == .suboptimal_khr) {
        backend.new_swapchain_extent = backend.present_info.surface_extent;
        log.warn("present suboptimal, requesting swapchain re-creation", .{});
    } else {
        std.debug.assert(present_result == .success);
    }

    return current_swapchain_index;
}
