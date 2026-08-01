// Port of src/GameLoop.cpp — the v1 subset.
//
// Event pumping, ESC-to-quit, resize forwarding, the ImGui frame and its three
// debug windows, and one frame execution per iteration.
//
// Not ported, all post-v1: the physics sim update, the free camera, and the
// Linux evdev controller. The keyboard-to-axis mapping is kept because it is
// what feeds the Controller Axes window — the axes just do not drive anything
// downstream yet.

const std = @import("std");
const log = std.log.scoped(.game);

const controller = @import("input/controller.zig");
const debug_ui = @import("renderer/debug_ui.zig");
const execute_frame = @import("renderer/vulkan/execute_frame.zig");
const imgui = @import("renderer/imgui.zig");
const scene_module = @import("renderer/scene.zig");
const screenshot = @import("renderer/vulkan/screenshot.zig");
const renderdoc = @import("renderer/renderdoc.zig");
const window_module = @import("renderer/window/window.zig");
const BackendResources = @import("renderer/vulkan/backend_resources.zig").BackendResources;
const VulkanBackend = @import("renderer/vulkan/Backend.zig").VulkanBackend;
const Window = window_module.Window;

/// ImGuiScrollMultiplier in GameLoop.cpp.
const imgui_scroll_multiplier: f32 = 0.5;

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
    /// Wrap this frame in a RenderDoc capture. 1-based, matching frame_count.
    /// Needs --renderdoc, since the library has to be loaded before the Vulkan
    /// instance exists and that decision is made long before this runs.
    capture_frame: ?u32 = null,
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

    if (options.capture_frame != null and !renderdoc.isAvailable()) {
        log.err("--capture-frame needs --renderdoc, and RenderDoc has to be installed", .{});
        return error.RenderDocUnavailable;
    }

    var events: std.ArrayList(window_module.Event) = .empty;
    defer events.deinit(allocator);

    var should_exit = false;
    var presented_frame_count: u32 = 0;
    var readback: ?screenshot.Readback = null;
    defer if (readback) |*r| r.deinit(backend.vkd, backend.device);

    var controller_state = controller.State.neutral;
    var physics_ui_state = debug_ui.PhysicsUiState{};

    while (!should_exit) {
        // The mouse position is polled rather than event-driven, same as the
        // C++; only buttons and the wheel arrive as events.
        const mouse_state = window.getMouseState();
        imgui.setDisplaySize(
            @floatFromInt(backend.present_info.surface_extent.width),
            @floatFromInt(backend.present_info.surface_extent.height),
        );
        imgui.addMousePosEvent(@floatFromInt(mouse_state.pos_x), @floatFromInt(mouse_state.pos_y));

        imgui.vulkanNewFrame();
        imgui.newFrame();

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
            .mouse_button => |mouse_button| {
                log.debug("window: button = {s}, press = {}", .{
                    window_module.getMouseButtonString(mouse_button.button),
                    mouse_button.press,
                });

                switch (mouse_button.button) {
                    .left => imgui.addMouseButtonEvent(0, mouse_button.press),
                    .right => imgui.addMouseButtonEvent(1, mouse_button.press),
                    .middle => imgui.addMouseButtonEvent(2, mouse_button.press),
                    .invalid => {},
                }
            },
            .mouse_wheel => |mouse_wheel| {
                log.debug("window: mouse wheel event, x = {}, y = {}", .{ mouse_wheel.x_delta, mouse_wheel.y_delta });

                imgui.addMouseWheelEvent(
                    @as(f32, @floatFromInt(mouse_wheel.x_delta)) * imgui_scroll_multiplier,
                    @as(f32, @floatFromInt(mouse_wheel.y_delta)) * imgui_scroll_multiplier,
                );
            },
            .key_press => |key_press| {
                log.debug("window: key = {s}, press = {}, scancode = {}", .{
                    window_module.getKeyboardKeyString(key_press.key),
                    key_press.press,
                    key_press.internal_key_code,
                });

                const is_pressed = key_press.press;

                if (is_pressed and key_press.key == .escape) {
                    log.warn("window: escape key press detected: now exiting...", .{});
                    should_exit = true;
                } else switch (key_press.key) {
                    // FIXME key auto-repeat would ruin this for us, which is
                    // why window.zig drops repeats outright.
                    .a => controller_state.set(.lsx, if (is_pressed) -1.0 else 0.0),
                    .d => controller_state.set(.lsx, if (is_pressed) 1.0 else 0.0),
                    .w => controller_state.set(.lsy, if (is_pressed) -1.0 else 0.0),
                    .s => controller_state.set(.lsy, if (is_pressed) 1.0 else 0.0),
                    .arrow_left => controller_state.set(.rsx, if (is_pressed) -1.0 else 0.0),
                    .arrow_right => controller_state.set(.rsx, if (is_pressed) 1.0 else 0.0),
                    .arrow_up => {
                        controller_state.set(.rsy, if (is_pressed) -1.0 else 0.0);
                        controller_state.set(.rt, if (is_pressed) 1.0 else -1.0);
                    },
                    .arrow_down => {
                        controller_state.set(.rsy, if (is_pressed) 1.0 else 0.0);
                        controller_state.set(.lt, if (is_pressed) 1.0 else -1.0);
                    },
                    else => {},
                }
            },
        };

        debug_ui.controllerDebugUi(controller_state);
        debug_ui.backendDebugUi(backend);

        physics_ui_state.player_translation = scene_module.getPlayerTranslation(scene);
        debug_ui.physicsDebugUi(&physics_ui_state);

        // Everything above may have queued draw commands; from here on the
        // draw data is what the GUI pass will consume.
        imgui.render();

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

        // Brackets the whole frame including the present, so the capture holds
        // the swapchain image the compositor actually received.
        const is_renderdoc_frame = options.capture_frame != null and
            presented_frame_count + 1 == options.capture_frame.?;

        if (is_renderdoc_frame) {
            renderdoc.startCapture(backend.instance);
        }
        defer if (is_renderdoc_frame) renderdoc.endCapture(backend.instance);

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
