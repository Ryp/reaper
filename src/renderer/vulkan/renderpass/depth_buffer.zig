// The main pass depth buffer.
//
// The C++ declares this inline in TestGraphics.cpp rather than in a pass of its
// own, because several passes share it. Same idea here, just factored out so
// the forward pass does not have to own it.

const vk = @import("vulkan");

const constants = @import("constants.zig");
const fg = @import("../../graph/frame_graph.zig");
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");

const Builder = @import("../../graph/builder.zig").Builder;

pub fn createFrameGraphRecord(
    builder: *Builder,
    render_extent: vk.Extent2D,
) !struct { pass_handle: fg.RenderPassHandle, depth: fg.ResourceUsageHandle } {
    const pass_handle = try builder.createRenderPass("Depth Buffer", false);

    const depth = try builder.createTexture(
        pass_handle,
        "Main Depth",
        gpu_texture_properties.defaultTextureProperties(
            render_extent.width,
            render_extent.height,
            constants.main_pass_depth_format,
            .{ .depth_stencil_attachment = true, .sampled = true },
        ),
        .{
            .stage_mask = .{ .early_fragment_tests_bit = true },
            .access_mask = .{ .depth_stencil_attachment_write_bit = true },
            .image_layout = .attachment_optimal,
        },
        &.{},
    );

    return .{ .pass_handle = pass_handle, .depth = depth };
}
