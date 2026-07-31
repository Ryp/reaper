// Zig-side of the Dear ImGui binding.
//
// The C surface lives in imgui_shim.{h,cpp} because @cImport cannot read
// imgui.h; this file is the ergonomic layer over it — Zig slices instead of
// NUL-terminated pointers, a formatting `text()` in place of ImGui::Text's
// varargs, and vulkan-zig handles converted to the raw values the shim takes.

const std = @import("std");
const vk = @import("vulkan");

pub const c = @cImport({
    @cInclude("imgui_shim.h");
});

pub const cond_first_use_ever = c.RIMGUI_COND_FIRST_USE_EVER;
pub const slider_flags_logarithmic = c.RIMGUI_SLIDER_FLAGS_LOGARITHMIC;
pub const input_text_flags_read_only = c.RIMGUI_INPUT_TEXT_FLAGS_READ_ONLY;

pub const Vec2 = struct { x: f32, y: f32 };

/// Scratch for the label/text formatting helpers. ImGui labels are short by
/// construction, and this keeps the UI code free of allocator plumbing.
var format_buffer: [512]u8 = undefined;

fn cstr(comptime fmt: []const u8, args: anytype) [*:0]const u8 {
    const formatted = std.fmt.bufPrintZ(&format_buffer, fmt, args) catch blk: {
        format_buffer[format_buffer.len - 1] = 0;
        break :blk format_buffer[0 .. format_buffer.len - 1 :0];
    };
    return formatted.ptr;
}

// --------------------------------------------------------------------------
// Context and Vulkan backend
// --------------------------------------------------------------------------

pub fn createContext() void {
    c.rimgui_create_context();
}

pub fn destroyContext() void {
    c.rimgui_destroy_context();
}

pub const VulkanInitInfo = struct {
    instance: vk.Instance,
    physical_device: vk.PhysicalDevice,
    device: vk.Device,
    queue_family: u32,
    queue: vk.Queue,
    descriptor_pool: vk.DescriptorPool,
    min_image_count: u32,
    image_count: u32,
    color_attachment_format: vk.Format,
};

/// vulkan-zig models dispatchable handles as `enum(usize)` and non-dispatchable
/// ones as `enum(u64)`, so both directions of the shim boundary go through the
/// integer value rather than a pointer cast on the enum itself.
fn handleToPtr(handle: anytype) ?*anyopaque {
    return @ptrFromInt(@intFromEnum(handle));
}

pub fn vulkanInit(info: VulkanInitInfo) !void {
    const shim_info = c.RImGuiVulkanInitInfo{
        .instance = handleToPtr(info.instance),
        .physical_device = handleToPtr(info.physical_device),
        .device = handleToPtr(info.device),
        .queue_family = info.queue_family,
        .queue = handleToPtr(info.queue),
        .descriptor_pool = @intFromEnum(info.descriptor_pool),
        .min_image_count = info.min_image_count,
        .image_count = info.image_count,
        .color_attachment_format = @intFromEnum(info.color_attachment_format),
    };

    if (!c.rimgui_vulkan_init(&shim_info)) return error.ImGuiVulkanInitFailed;
}

pub fn vulkanShutdown() void {
    c.rimgui_vulkan_shutdown();
}

pub fn vulkanNewFrame() void {
    c.rimgui_vulkan_new_frame();
}

pub fn vulkanCreateFontsTexture(cmd_buffer: vk.CommandBuffer) !void {
    if (!c.rimgui_vulkan_create_fonts_texture(handleToPtr(cmd_buffer))) return error.ImGuiFontUploadFailed;
}

pub fn vulkanDestroyFontUploadObjects() void {
    c.rimgui_vulkan_destroy_font_upload_objects();
}

pub fn vulkanRenderDrawData(cmd_buffer: vk.CommandBuffer) void {
    c.rimgui_vulkan_render_draw_data(handleToPtr(cmd_buffer));
}

// --------------------------------------------------------------------------
// Frame
// --------------------------------------------------------------------------

pub fn newFrame() void {
    c.rimgui_new_frame();
}

pub fn render() void {
    c.rimgui_render();
}

// --------------------------------------------------------------------------
// IO
// --------------------------------------------------------------------------

pub fn setDisplaySize(width: f32, height: f32) void {
    c.rimgui_io_set_display_size(width, height);
}

pub fn addMousePosEvent(x: f32, y: f32) void {
    c.rimgui_io_add_mouse_pos_event(x, y);
}

pub fn addMouseButtonEvent(button_index: i32, down: bool) void {
    c.rimgui_io_add_mouse_button_event(button_index, down);
}

pub fn addMouseWheelEvent(x: f32, y: f32) void {
    c.rimgui_io_add_mouse_wheel_event(x, y);
}

// --------------------------------------------------------------------------
// Windows and layout
// --------------------------------------------------------------------------

pub fn getMainViewportWorkPos() Vec2 {
    var x: f32 = 0;
    var y: f32 = 0;
    c.rimgui_get_main_viewport_work_pos(&x, &y);
    return .{ .x = x, .y = y };
}

pub fn setNextWindowPos(pos: Vec2, cond: i32) void {
    c.rimgui_set_next_window_pos(pos.x, pos.y, cond);
}

pub fn setNextWindowBgAlpha(alpha: f32) void {
    c.rimgui_set_next_window_bg_alpha(alpha);
}

pub fn begin(name: [*:0]const u8, p_open: ?*bool) bool {
    return c.rimgui_begin(name, p_open);
}

pub fn end() void {
    c.rimgui_end();
}

pub fn beginChild(str_id: [*:0]const u8, size: Vec2) bool {
    return c.rimgui_begin_child(str_id, size.x, size.y);
}

pub fn endChild() void {
    c.rimgui_end_child();
}

pub fn sameLine(offset_from_start_x: f32) void {
    c.rimgui_same_line(offset_from_start_x);
}

pub fn separator() void {
    c.rimgui_separator();
}

// --------------------------------------------------------------------------
// Widgets
// --------------------------------------------------------------------------

/// Stands in for ImGui::Text(fmt, ...): the formatting happens here and the
/// result goes through TextUnformatted, so no varargs cross the FFI boundary.
pub fn text(comptime fmt: []const u8, args: anytype) void {
    c.rimgui_text_unformatted(cstr(fmt, args));
}

pub fn button(label: [*:0]const u8) bool {
    return c.rimgui_button(label);
}

pub fn checkbox(label: [*:0]const u8, v: *bool) bool {
    return c.rimgui_checkbox(label, v);
}

pub fn beginDisabled(disabled: bool) void {
    c.rimgui_begin_disabled(disabled);
}

pub fn endDisabled() void {
    c.rimgui_end_disabled();
}

pub fn sliderFloat(label: [*:0]const u8, v: *f32, v_min: f32, v_max: f32) bool {
    return c.rimgui_slider_float(label, v, v_min, v_max, "%.3f", 0);
}

pub fn sliderFloatEx(
    label: [*:0]const u8,
    v: *f32,
    v_min: f32,
    v_max: f32,
    format: [*:0]const u8,
    flags: i32,
) bool {
    return c.rimgui_slider_float(label, v, v_min, v_max, format, flags);
}

pub fn sliderInt(label: [*:0]const u8, v: *i32, v_min: i32, v_max: i32) bool {
    return c.rimgui_slider_int(label, v, v_min, v_max);
}

pub fn sliderU32(label: [*:0]const u8, v: *u32, v_min: u32, v_max: u32, format: [*:0]const u8) bool {
    return c.rimgui_slider_u32(label, v, v_min, v_max, format);
}

pub fn inputFloat3(label: [*:0]const u8, v: *[3]f32, format: [*:0]const u8, flags: i32) bool {
    return c.rimgui_input_float3(label, v, format, flags);
}

// --------------------------------------------------------------------------
// Raw draw list
// --------------------------------------------------------------------------

/// ImGui's IM_COL32, which packs to ABGR — the byte order the vertex format
/// expects, not RGBA.
pub fn col32(r: u8, g: u8, b: u8, a: u8) u32 {
    return @as(u32, r) | (@as(u32, g) << 8) | (@as(u32, b) << 16) | (@as(u32, a) << 24);
}

pub fn getCursorScreenPos() Vec2 {
    var x: f32 = 0;
    var y: f32 = 0;
    c.rimgui_get_cursor_screen_pos(&x, &y);
    return .{ .x = x, .y = y };
}

pub fn drawRectFilled(min: Vec2, max: Vec2, color: u32) void {
    c.rimgui_draw_rect_filled(min.x, min.y, max.x, max.y, color);
}

pub fn drawRect(min: Vec2, max: Vec2, color: u32) void {
    c.rimgui_draw_rect(min.x, min.y, max.x, max.y, color);
}

pub fn drawLine(p0: Vec2, p1: Vec2, color: u32, thickness: f32) void {
    c.rimgui_draw_line(p0.x, p0.y, p1.x, p1.y, color, thickness);
}

pub fn drawCircle(center: Vec2, radius: f32, color: u32, num_segments: i32) void {
    c.rimgui_draw_circle(center.x, center.y, radius, color, num_segments);
}

pub fn drawCircleFilled(center: Vec2, radius: f32, color: u32) void {
    c.rimgui_draw_circle_filled(center.x, center.y, radius, color);
}
