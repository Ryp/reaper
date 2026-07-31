// Port of src/renderer/vulkan/Debug.{h,cpp}
//
// VK_EXT_debug_utils command-buffer labels and object names. These are what a
// capture tool shows: labels become the collapsible scopes that let you find a
// draw call in RenderDoc's event browser, and object names replace
// "VkImage 0x2330000000233" with "GBuffer RT0".
//
// DEVIATION from Backend.zig's original wiring, which requested the extension
// only when validation was on. Captures are usually taken against a Release
// build, and that is exactly when unnamed scopes hurt most, so availability is
// now decided by the driver rather than by the build mode. Everything here is
// a no-op when the extension is missing.

const std = @import("std");
const vk = @import("vulkan");

/// Set once at instance creation. The dispatch entries are null when the
/// extension was not enabled, and vulkan-zig unwraps them with `.?`, so every
/// entry point here has to check first.
var available: bool = false;

pub fn setAvailable(value: bool) void {
    available = value;
}

pub fn isAvailable() bool {
    return available;
}

/// Names a Vulkan object for capture tools. `handle` is the raw 64-bit value;
/// vulkan-zig handles are enums over usize/u64, so callers pass
/// `@intFromEnum(h)`.
pub fn setObjectName(
    vkd: anytype,
    device: vk.Device,
    object_type: vk.ObjectType,
    handle: u64,
    name: [*:0]const u8,
) void {
    if (!available) return;

    const name_info = vk.DebugUtilsObjectNameInfoEXT{
        .s_type = .debug_utils_object_name_info_ext,
        .p_next = null,
        .object_type = object_type,
        .object_handle = handle,
        .p_object_name = name,
    };

    // Naming is diagnostics only — a failure here must never take the frame
    // down, which is why this swallows where the C++ AssertVk's (Debug.cpp:30).
    vkd.setDebugUtilsObjectNameEXT(device, &name_info) catch {};
}

pub fn begin(vkd: anytype, cmd_buffer: vk.CommandBuffer, name: [*:0]const u8) void {
    if (!available) return;

    const label = vk.DebugUtilsLabelEXT{
        .s_type = .debug_utils_label_ext,
        .p_next = null,
        .p_label_name = name,
        .color = .{ 0.0, 0.0, 0.0, 0.0 },
    };

    vkd.cmdBeginDebugUtilsLabelEXT(cmd_buffer, &label);
}

pub fn end(vkd: anytype, cmd_buffer: vk.CommandBuffer) void {
    if (!available) return;

    vkd.cmdEndDebugUtilsLabelEXT(cmd_buffer);
}

pub fn insert(vkd: anytype, cmd_buffer: vk.CommandBuffer, name: [*:0]const u8) void {
    if (!available) return;

    const label = vk.DebugUtilsLabelEXT{
        .s_type = .debug_utils_label_ext,
        .p_next = null,
        .p_label_name = name,
        .color = .{ 0.0, 0.0, 0.0, 0.0 },
    };

    vkd.cmdInsertDebugUtilsLabelEXT(cmd_buffer, &label);
}
