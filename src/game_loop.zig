// Port of src/GameLoop.cpp — the v1 subset.
//
// M0 only carries the parts of the loop that exist without a scene: event
// pumping, ESC-to-quit, resize forwarding, and one frame execution per
// iteration. Scene setup, ImGui, and controller handling land in later
// milestones.

const std = @import("std");
const log = std.log.scoped(.game);

const execute_frame = @import("renderer/vulkan/execute_frame.zig");
const scene_module = @import("renderer/scene.zig");
const screenshot = @import("renderer/vulkan/screenshot.zig");
const window_module = @import("renderer/window/window.zig");
const BackendResources = @import("renderer/vulkan/backend_resources.zig").BackendResources;
const VulkanBackend = @import("renderer/vulkan/Backend.zig").VulkanBackend;
const Window = window_module.Window;

/// Milliseconds to idle for while the window is minimized. The swapchain
/// extent is degenerate then, so there is nothing worth submitting.
const minimized_sleep_ms = 16;

pub const Options = struct {
    /// Quit after this many presented frames. Null means run until the user
    /// asks to exit; set from --frame-count for scripted runs.
    frame_count: ?u32 = null,
    /// Dump the last frame here before quitting. Requires frame_count, since
    /// the capture has to be recorded into that frame's command buffer.
    screenshot_path: ?[]const u8 = null,
};

pub fn run(
    allocator: std.mem.Allocator,
    io: std.Io,
    window: *Window,
    backend: *VulkanBackend,
    resources: *BackendResources,
    scene: *const scene_module.Scene,
    options: Options,
) !void {
    if (options.screenshot_path != null and options.frame_count == null) {
        log.err("--screenshot needs --frame-count to know which frame to capture", .{});
        return error.MissingFrameCount;
    }

    var events: std.ArrayList(window_module.Event) = .empty;
    defer events.deinit(allocator);

    var should_exit = false;
    var presented_frame_count: u32 = 0;
    var readback: ?screenshot.Readback = null;
    defer if (readback) |*r| r.deinit(backend.vkd, backend.device);

    while (!should_exit) {
        events.clearRetainingCapacity();
        try window.pumpEvents(allocator, &events);

        for (events.items) |event| switch (event) {
            .close => {
                log.info("window close requested: now exiting...", .{});
                should_exit = true;
            },
            .resize => |resize| {
                log.debug("window: resize event, width = {}, height = {}", .{ resize.width, resize.height });

                std.debug.assert(resize.width > 0);
                std.debug.assert(resize.height > 0);

                // FIXME Do not set for duplicate events
                backend.new_swapchain_extent = .{ .width = resize.width, .height = resize.height };
            },
            .key_press => |key_press| {
                log.debug("window: key = {s}, press = {}, scancode = {}", .{
                    window_module.getKeyboardKeyString(key_press.key),
                    key_press.press,
                    key_press.internal_key_code,
                });

                if (key_press.press and key_press.key == .escape) {
                    log.warn("window: escape key press detected: now exiting...", .{});
                    should_exit = true;
                }
            },
            // Mouse events only matter once ImGui is wired up.
            .mouse_button, .mouse_wheel => {},
        };

        if (should_exit) break;

        if (window.isMinimized()) {
            window_module.delay(minimized_sleep_ms);
            continue;
        }

        try execute_frame.resizeSwapchain(backend);

        // The capture is recorded into the last frame's command buffer, so the
        // staging buffer has to exist before that frame starts.
        const is_capture_frame = options.screenshot_path != null and
            presented_frame_count + 1 == options.frame_count.?;

        if (is_capture_frame) {
            readback = try screenshot.Readback.init(
                backend.vkd,
                backend.device,
                backend.physical_device.memory_properties,
                backend.present_info.surface_extent,
                backend.present_info.swapchain_format.vk_view_format,
            );
        }

        // A frame failing is not fatal: log it and try the next one, same as
        // the C++ loop's log-and-continue behavior.
        const presented_image_index = execute_frame.executeFrame(
            backend,
            resources,
            scene,
            if (is_capture_frame) &readback.? else null,
        ) catch |err| blk: {
            log.err("frame execution failed: {}", .{err});
            break :blk null;
        };

        if (presented_image_index == null) {
            // The frame was dropped, so nothing was captured either.
            if (readback) |*r| {
                r.deinit(backend.vkd, backend.device);
                readback = null;
            }
            continue;
        }

        presented_frame_count += 1;

        if (readback) |*r| {
            try backend.vkd.queueWaitIdle(backend.graphics_queue);
            try screenshot.write(backend.vkd, backend.device, r, allocator, io, options.screenshot_path.?);
        }

        if (options.frame_count) |frame_count| {
            if (presented_frame_count >= frame_count) {
                log.info("reached the requested frame count ({}): now exiting...", .{frame_count});
                should_exit = true;
            }
        }
    }
}
