// Replaces src/renderer/vulkan/api/VulkanStringConversion.{h,cpp}.
//
// The C++ file is ~700 lines of hand-written `switch` returning string
// literals, one arm per enumerant. vulkan-zig already carries every enumerant
// name as a Zig tag, so all that is left is spelling it back the Vulkan way:
// uppercase the tag and put the enum's prefix in front.
//
// Only the conversions the renderer actually calls are exposed. The C++ also
// converts VkResult, VkPresentModeKHR, VkPhysicalDeviceType, VkImageLayout and
// VkObjectType; Zig prints those with `{s}`/`{t}` on the tag directly wherever
// they are logged, so they never needed a helper.

const std = @import("std");
const vk = @import("vulkan");

/// Written to by every call; the result is only valid until the next one, which
/// is fine for the one-shot ImGui::Text and log calls that use it.
var name_buffer: [128]u8 = undefined;

fn vkEnumName(comptime T: type, value: T, comptime prefix: []const u8) []const u8 {
    const tag = std.enums.tagName(T, value) orelse {
        return std.fmt.bufPrint(&name_buffer, "{s}UNKNOWN({d})", .{
            prefix,
            @intFromEnum(value),
        }) catch prefix ++ "UNKNOWN";
    };

    if (prefix.len + tag.len > name_buffer.len) return tag;

    @memcpy(name_buffer[0..prefix.len], prefix);
    for (name_buffer[prefix.len..][0..tag.len], tag) |*out, char| {
        out.* = std.ascii.toUpper(char);
    }

    return name_buffer[0 .. prefix.len + tag.len];
}

pub fn formatName(format: vk.Format) []const u8 {
    return vkEnumName(vk.Format, format, "VK_FORMAT_");
}

pub fn colorSpaceName(color_space: vk.ColorSpaceKHR) []const u8 {
    return vkEnumName(vk.ColorSpaceKHR, color_space, "VK_COLOR_SPACE_");
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "names match VulkanStringConversion's spelling" {
    try testing.expectEqualStrings("VK_FORMAT_B8G8R8A8_SRGB", formatName(.b8g8r8a8_srgb));
    try testing.expectEqualStrings("VK_FORMAT_A2B10G10R10_UNORM_PACK32", formatName(.a2b10g10r10_unorm_pack32));
    try testing.expectEqualStrings("VK_FORMAT_UNDEFINED", formatName(.undefined));
    try testing.expectEqualStrings("VK_COLOR_SPACE_SRGB_NONLINEAR_KHR", colorSpaceName(.srgb_nonlinear_khr));
    try testing.expectEqualStrings("VK_COLOR_SPACE_HDR10_ST2084_EXT", colorSpaceName(.hdr10_st2084_ext));
}

test "an enumerant with no tag falls back to the numeric value" {
    // vk.Format is non-exhaustive, so a value from an extension newer than the
    // registry can reach this. @tagName would panic where this must not.
    const unknown: vk.Format = @enumFromInt(0x7000_0000);

    try testing.expectEqualStrings("VK_FORMAT_UNKNOWN(1879048192)", formatName(unknown));
}
