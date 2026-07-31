// Port of src/renderer/vulkan/RenderPassHelpers.h

const vk = @import("vulkan");

pub fn defaultViewport(output_rect: vk.Rect2D) vk.Viewport {
    return .{
        .x = @floatFromInt(output_rect.offset.x),
        .y = @floatFromInt(output_rect.offset.y),
        .width = @floatFromInt(output_rect.extent.width),
        .height = @floatFromInt(output_rect.extent.height),
        .min_depth = 0.0,
        .max_depth = 1.0,
    };
}

pub fn defaultRect(image_extent: vk.Extent2D) vk.Rect2D {
    return .{
        .offset = .{ .x = 0, .y = 0 },
        .extent = image_extent,
    };
}
