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
const screenshot = @import("screenshot.zig");
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

const swapchain_access_render = barrier.GPUTextureAccess{
    .stage_mask = .{ .color_attachment_output_bit = true },
    .access_mask = .{ .color_attachment_write_bit = true },
    .image_layout = .attachment_optimal,
};

// NOTE: M0 clears through an empty dynamic-rendering pass rather than
// vkCmdClearColorImage. The latter would need VK_IMAGE_USAGE_TRANSFER_DST_BIT
// on the swapchain, which the C++ backend never requests; going through an
// attachment keeps the swapchain configuration and the barrier layouts above
// identical to C++, and is the shape the real swapchain pass needs anyway.
const clear_color = vk.ClearColorValue{ .float_32 = .{ 0.1, 0.2, 0.4, 1.0 } };

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

    // A rendering scope with no draws in it: the LOAD_OP_CLEAR is the frame.
    {
        const color_attachments = [_]vk.RenderingAttachmentInfo{.{
            .s_type = .rendering_attachment_info,
            .p_next = null,
            .image_view = backend.present_info.image_views[current_swapchain_index],
            .image_layout = swapchain_access_render.image_layout,
            .resolve_mode = .{},
            .resolve_image_view = .null_handle,
            .resolve_image_layout = .undefined,
            .load_op = .clear,
            .store_op = .store,
            .clear_value = .{ .color = clear_color },
        }};

        const rendering_info = vk.RenderingInfo{
            .s_type = .rendering_info,
            .p_next = null,
            .flags = .{},
            .render_area = .{
                .offset = .{ .x = 0, .y = 0 },
                .extent = backend.present_info.surface_extent,
            },
            .layer_count = 1,
            .view_mask = 0,
            .color_attachment_count = color_attachments.len,
            .p_color_attachments = &color_attachments,
            .p_depth_attachment = null,
            .p_stencil_attachment = null,
        };

        vkd.cmdBeginRendering(cmd_buffer, &rendering_info);
        vkd.cmdEndRendering(cmd_buffer);
    }

    // The readback has to happen here, while the frame still owns the image:
    // transitioning an image that has already been presented is invalid.
    if (readback) |readback_target| {
        const to_copy = [_]vk.ImageMemoryBarrier2{
            barrier.getImageBarrierSameQueue(swapchain_image, subresource, swapchain_access_render, screenshot.access_copy),
        };
        vkd.cmdPipelineBarrier2(cmd_buffer, &barrier.getImageBarrierDependencyInfo(&to_copy));

        screenshot.recordCopy(vkd, cmd_buffer, swapchain_image, readback_target);
    }

    // Back to PRESENT_SRC before handing the image to the presentation engine.
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
