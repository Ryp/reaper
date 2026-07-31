// Port of src/renderer/vulkan/renderpass/SwapchainPass.cpp — M1 subset.
//
// The final pass composites the HDR scene, the GUI and the tonemapping LUT
// through swapchain_write.frag, which needs seven descriptors and a push
// constant block that none of those resources exist for yet. M1 keeps the real
// pipeline shape — same vertex shader, same default pipeline state, same
// dynamic viewport/scissor, same 3-vertex draw — and swaps in a fragment shader
// that needs no inputs, so this is a genuine end-to-end test of the shader
// build chain and the pipeline plumbing.
//
// The pipeline lives outside the pipeline factory because it depends on the
// swapchain view format and must be rebuilt when the swapchain is reconfigured.

const std = @import("std");
const vk = @import("vulkan");

const pipeline_module = @import("../pipeline.zig");
const render_pass_helpers = @import("../render_pass_helpers.zig");
const shader_modules = @import("../shader_modules.zig");

pub const SwapchainPassResources = struct {
    pipeline_layout: vk.PipelineLayout,
    pipeline: vk.Pipeline,
    /// The swapchain view format the pipeline was built against.
    output_format: vk.Format,

    pub fn init(vkd: anytype, device: vk.Device, output_format: vk.Format) !SwapchainPassResources {
        // M1's fragment shader reads nothing, so the layout is empty. The real
        // pass adds one descriptor set and a fragment push constant range.
        const pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &.{}, &.{});
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const pipeline = try createPipeline(vkd, device, pipeline_layout, output_format);

        return .{
            .pipeline_layout = pipeline_layout,
            .pipeline = pipeline,
            .output_format = output_format,
        };
    }

    pub fn deinit(self: *SwapchainPassResources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipeline(device, self.pipeline, null);
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
    }

    /// Rebuilds the pipeline if the swapchain came back with a different view
    /// format. The caller must have waited for the device to go idle.
    pub fn reconfigure(self: *SwapchainPassResources, vkd: anytype, device: vk.Device, output_format: vk.Format) !void {
        if (self.output_format == output_format) return;

        const pipeline = try createPipeline(vkd, device, self.pipeline_layout, output_format);

        vkd.destroyPipeline(device, self.pipeline, null);
        self.pipeline = pipeline;
        self.output_format = output_format;
    }
};

fn createPipeline(
    vkd: anytype,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
    output_format: vk.Format,
) !vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("fullscreen_triangle.vert.spv"),
    );
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("gui_write.frag.spv"),
    );

    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .fragment_bit = true }, &module_create_info_frag, null),
    };

    const blend_attachment_states = [_]vk.PipelineColorBlendAttachmentState{
        pipeline_module.defaultPipelineColorBlendAttachmentState(),
    };

    const color_attachment_formats = [_]vk.Format{output_format};

    var properties = pipeline_module.defaultGraphicsPipelineProperties(null);
    properties.blend_state.attachment_count = blend_attachment_states.len;
    properties.blend_state.p_attachments = &blend_attachment_states;
    properties.pipeline_layout = pipeline_layout;
    properties.pipeline_rendering.color_attachment_count = color_attachment_formats.len;
    properties.pipeline_rendering.p_color_attachment_formats = &color_attachment_formats;

    const dynamic_states = [_]vk.DynamicState{ .viewport, .scissor };

    return pipeline_module.createGraphicsPipeline(vkd, device, &shader_stages, &properties, &dynamic_states);
}

/// Records the draw. The caller owns the rendering scope, matching how the
/// frame graph will drive this pass later.
pub fn record(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    resources: *const SwapchainPassResources,
    swapchain_extent: vk.Extent2D,
) void {
    const pass_rect = render_pass_helpers.defaultRect(swapchain_extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, resources.pipeline);

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    vkd.cmdDraw(cmd_buffer, 3, 1, 0, 0);
}
