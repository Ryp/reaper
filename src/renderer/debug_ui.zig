// The three debug windows: "Rendering" (backend_debug_ui in TestGraphics.cpp)
// and "Controller Axes" + "Physics" (imgui_controller_debug / imgui_sim_debug
// in GameLoop.cpp).
//
// Two deliberate differences from the C++, both because the backing system is
// post-v1 rather than because the UI changed:
//
//  - The Physics window drives a plain struct instead of a live PhysicsSim.
//    The values and ranges are the C++ ones, so the window looks and behaves
//    the same, but nothing reads them back yet.
//  - "Generate new track" is disabled: regenerating needs the mesh cache
//    cleared and every renderer mesh rebuilt, which v1 does not do.

const std = @import("std");

const controller = @import("../input/controller.zig");
const imgui = @import("imgui.zig");
const vk_string = @import("vulkan/vk_string.zig");

const VulkanBackend = @import("vulkan/Backend.zig").VulkanBackend;
const scene_module = @import("scene.zig");
const trackgen = @import("../neptune/trackgen.zig");

const imgui_white = imgui.col32(255, 255, 255, 255);
const imgui_gamepad_bg = imgui.col32(0, 30, 0, 255);

// --------------------------------------------------------------------------
// "Rendering"
// --------------------------------------------------------------------------

var show_rendering_window = true;

pub fn backendDebugUi(backend: *VulkanBackend) void {
    const work_pos = imgui.getMainViewportWorkPos();

    imgui.setNextWindowPos(.{ .x = work_pos.x + 10.0, .y = work_pos.y + 10.0 }, imgui.cond_first_use_ever);
    imgui.setNextWindowBgAlpha(0.35);

    if (imgui.begin("Rendering", &show_rendering_window)) {
        var swapchain_format = backend.present_info.swapchain_format;

        imgui.text("Swapchain info:", .{});
        imgui.text("format = {s}", .{vk_string.formatName(swapchain_format.vk_format)});
        imgui.text("colorspace = {s}", .{vk_string.colorSpaceName(swapchain_format.vk_color_space)});
        imgui.text("view format = {s}", .{vk_string.formatName(swapchain_format.vk_view_format)});

        imgui.beginDisabled(true);
        _ = imgui.checkbox("HDR Swapchain", &swapchain_format.is_hdr);
        // Inferred from the colour space above; the display's own answer can
        // disagree, and the tone mapping sliders below are guesses either way.
        var display_hdr = backend.display_hdr_enabled;
        _ = imgui.checkbox("HDR Display (reported)", &display_hdr);
        imgui.endDisabled();

        _ = imgui.checkbox("Freeze culling [BROKEN]", &backend.options.freeze_meshlet_culling); // FIXME
        _ = imgui.checkbox("Enable debug tile culling", &backend.options.enable_debug_tile_lighting);
        _ = imgui.checkbox("Enable MSAA-based visibility", &backend.options.enable_msaa_visibility);

        _ = imgui.sliderFloat("Tonemap min (nits)", &backend.present_info.tonemap_min_nits, 0.0001, 1.0);
        _ = imgui.sliderFloat("Tonemap max (nits)", &backend.present_info.tonemap_max_nits, 80.0, 2000.0);
        _ = imgui.sliderFloat("SDR UI max brightness (nits)", &backend.present_info.sdr_ui_max_brightness_nits, 20.0, 800.0);
        _ = imgui.sliderFloat("SDR peak brightness (nits)", &backend.present_info.sdr_peak_brightness_nits, 80.0, 2000.0);
        _ = imgui.sliderFloat("Exposure compensation (stops)", &backend.present_info.exposure_compensation_stops, -5.0, 5.0);
    }

    imgui.end();
}

// --------------------------------------------------------------------------
// "Controller Axes"
// --------------------------------------------------------------------------

var show_controller_window = true;

fn drawGamepadAxis(size_px: f32, axis_x: f32, axis_y: f32) void {
    const half_size_px = size_px * 0.5;

    const pos = imgui.getCursorScreenPos();

    // Draw safe region
    imgui.drawRectFilled(pos, .{ .x = pos.x + size_px, .y = pos.y + size_px }, imgui_gamepad_bg);

    // Draw frame and cross lines
    imgui.drawRect(pos, .{ .x = pos.x + size_px, .y = pos.y + size_px }, imgui_white);
    imgui.drawLine(
        .{ .x = pos.x + half_size_px, .y = pos.y },
        .{ .x = pos.x + half_size_px, .y = pos.y + size_px },
        imgui_white,
        1.0,
    );
    imgui.drawLine(
        .{ .x = pos.x, .y = pos.y + half_size_px },
        .{ .x = pos.x + size_px, .y = pos.y + half_size_px },
        imgui_white,
        1.0,
    );
    imgui.drawCircle(
        .{ .x = pos.x + half_size_px, .y = pos.y + half_size_px },
        half_size_px,
        imgui_white,
        32,
    );

    // Current position
    imgui.drawCircleFilled(
        .{
            .x = pos.x + axis_x * half_size_px + half_size_px,
            .y = pos.y + axis_y * half_size_px + half_size_px,
        },
        10,
        imgui_white,
    );
}

fn drawGamepadTrigger(w_px: f32, h_px: f32, trigger_axis: f32) void {
    const mag = trigger_axis * 0.5 + 0.5;
    const current_y = h_px * (1.0 - mag);

    const pos = imgui.getCursorScreenPos();

    // Draw safe region
    imgui.drawRectFilled(pos, .{ .x = pos.x + w_px, .y = pos.y + h_px }, imgui_gamepad_bg);

    // Draw frame
    imgui.drawRect(pos, .{ .x = pos.x + w_px, .y = pos.y + h_px }, imgui_white);

    // Current position
    imgui.drawLine(
        .{ .x = pos.x, .y = pos.y + current_y },
        .{ .x = pos.x + w_px, .y = pos.y + current_y },
        imgui_white,
        8.0,
    );
}

pub fn controllerDebugUi(state: controller.State) void {
    const work_pos = imgui.getMainViewportWorkPos();

    imgui.setNextWindowPos(.{ .x = work_pos.x + 300.0, .y = work_pos.y }, imgui.cond_first_use_ever);
    imgui.setNextWindowBgAlpha(0.35); // Transparent background

    if (imgui.begin("Controller Axes", &show_controller_window)) {
        const size_px: f32 = 200.0;
        const w_px: f32 = 40.0;

        _ = imgui.beginChild("LeftStick", .{ .x = size_px, .y = size_px });
        drawGamepadAxis(size_px, state.get(.lsx), state.get(.lsy));
        imgui.endChild();
        imgui.sameLine(size_px + 20.0);

        _ = imgui.beginChild("LeftTrigger", .{ .x = w_px, .y = size_px });
        drawGamepadTrigger(w_px, size_px, state.get(.lt));
        imgui.endChild();
        imgui.sameLine(size_px + w_px + 2.0 * 20.0);

        // Right pad.
        _ = imgui.beginChild("RightStick", .{ .x = size_px, .y = size_px });
        drawGamepadAxis(size_px, state.get(.rsx), state.get(.rsy));
        imgui.endChild();
        imgui.sameLine(size_px * 2.0 + w_px + 3.0 * 20.0);

        _ = imgui.beginChild("RightTrigger", .{ .x = w_px, .y = size_px });
        drawGamepadTrigger(w_px, size_px, state.get(.rt));
        imgui.endChild();
    }

    imgui.end();
}

// --------------------------------------------------------------------------
// "Physics"
// --------------------------------------------------------------------------

/// PhysicsSim::Vars, with the defaults from create_sim(). Nothing reads these
/// back in v1 — the sim is not ported — so the window is a faithful set of
/// controls over dead state rather than a live tuning panel.
pub const PhysicsSimVars = struct {
    simulation_substep_duration: f32 = 1.0 / 60.0,
    max_simulation_substep_count: i32 = 3,
    gravity_force_intensity: f32 = 9.8331,
    linear_friction: f32 = 0.4,
    angular_friction: f32 = 0.999,
    enable_suspension_forces: bool = true,
    max_suspension_force: f32 = 6000.0,
    default_spring_stiffness: f32 = 30.0,
    default_damper_friction_compression: f32 = 2.82,
    default_damper_friction_extension: f32 = 0.22,
    enable_debug_geometry: bool = false,
    steer_force: f32 = 2.0,
    ship_thrust: f32 = 20.0,
    ship_braking: f32 = 10.0,
    ship_handling: f32 = 0.4,

    /// RaycastSuspension::length_ratio_last, read-only in the C++ too.
    suspension_length_ratio_last: [4]f32 = @splat(0.0),
};

const log_slider = imgui.slider_flags_logarithmic;

fn simDebugUi(vars: *PhysicsSimVars) void {
    _ = imgui.sliderFloat("simulation_substep_duration", &vars.simulation_substep_duration, 1.0 / 200.0, 1.0 / 5.0);
    _ = imgui.sliderInt("max_simulation_substep_count", &vars.max_simulation_substep_count, 1, 10);
    _ = imgui.sliderFloatEx("gravity_force_intensity", &vars.gravity_force_intensity, 0.0, 100.0, "%.3f", log_slider);
    _ = imgui.sliderFloatEx("linear_friction", &vars.linear_friction, 0.04, 1.0, "%.3f", log_slider);
    _ = imgui.sliderFloatEx("angular_friction", &vars.angular_friction, 0.04, 1.0, "%.3f", log_slider);
    _ = imgui.checkbox("enable_suspension_forces", &vars.enable_suspension_forces);
    _ = imgui.sliderFloatEx("max_suspension_force", &vars.max_suspension_force, 0.0, 100000.0, "%.3f", log_slider);
    _ = imgui.sliderFloatEx("default_spring_stiffness", &vars.default_spring_stiffness, 0.0, 100.0, "%.3f", log_slider);
    _ = imgui.sliderFloatEx(
        "default_damper_friction_compression",
        &vars.default_damper_friction_compression,
        0.0,
        100.0,
        "%.3f",
        log_slider,
    );
    _ = imgui.sliderFloatEx(
        "default_damper_friction_extension",
        &vars.default_damper_friction_extension,
        0.0,
        100.0,
        "%.3f",
        log_slider,
    );
    _ = imgui.checkbox("enable_debug_geometry", &vars.enable_debug_geometry);

    imgui.beginDisabled(true);
    _ = imgui.sliderFloat("suspension ratio 0", &vars.suspension_length_ratio_last[0], 0.0, 1.0);
    _ = imgui.sliderFloat("suspension ratio 1", &vars.suspension_length_ratio_last[1], 0.0, 1.0);
    _ = imgui.sliderFloat("suspension ratio 2", &vars.suspension_length_ratio_last[2], 0.0, 1.0);
    _ = imgui.sliderFloat("suspension ratio 3", &vars.suspension_length_ratio_last[3], 0.0, 1.0);
    imgui.endDisabled();

    _ = imgui.sliderFloatEx("steer_force", &vars.steer_force, 0.01, 100.0, "%.3f", log_slider);
    _ = imgui.sliderFloat("default_ship_stats.thrust", &vars.ship_thrust, 0.0, 1000.0);
    _ = imgui.sliderFloat("default_ship_stats.braking", &vars.ship_braking, 0.0, 100.0);
    _ = imgui.sliderFloat("default_ship_stats.handling", &vars.ship_handling, 0.1, 10.0);
}

var show_physics_window = true;

pub const PhysicsUiState = struct {
    sim_vars: PhysicsSimVars = .{},
    /// Opens on the parameters createGameScene() actually generated with.
    track_gen_info: trackgen.GenerationInfo = scene_module.track_gen_info,
    /// Read-only readout of where the player node currently is.
    player_translation: [3]f32 = @splat(0.0),
};

pub fn physicsDebugUi(state: *PhysicsUiState) void {
    const length_min: u32 = 1;
    const length_max: u32 = 100;

    const work_pos = imgui.getMainViewportWorkPos();

    imgui.setNextWindowPos(.{ .x = work_pos.x, .y = work_pos.y + 300.0 }, imgui.cond_first_use_ever);
    imgui.setNextWindowBgAlpha(0.35); // Transparent background

    if (imgui.begin("Physics", &show_physics_window)) {
        _ = imgui.inputFloat3("ship position", &state.player_translation, "%.3f", imgui.input_text_flags_read_only);
        imgui.separator();

        _ = imgui.sliderU32("chunk_count", &state.track_gen_info.chunk_count, length_min, length_max, "%u");
        _ = imgui.sliderFloat("radius_min_meter", &state.track_gen_info.radius_min_meter, 100.0, 2000.0);
        _ = imgui.sliderFloat("radius_max_meter", &state.track_gen_info.radius_max_meter, 100.0, 2000.0);
        _ = imgui.sliderFloat("chaos", &state.track_gen_info.chaos, 0.0, 1.0);

        // Regenerating the track needs every renderer mesh rebuilt, because
        // the mesh cache does not handle fragmentation. v1 builds the scene
        // once at startup, so the button is present but inert.
        imgui.beginDisabled(true);
        _ = imgui.button("Generate new track");
        imgui.endDisabled();

        imgui.separator();
        simDebugUi(&state.sim_vars);
    }

    imgui.end();
}
