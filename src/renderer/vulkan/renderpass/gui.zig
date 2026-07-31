// Port of src/renderer/vulkan/renderpass/GuiPass.{h,cpp}
//
// Renders the ImGui draw lists into their own SDR texture, which the swapchain
// composite then blends over the tone mapped scene. The pass owns a pipeline of
// its own that it binds but never draws with — ImGui brings its own — kept
// verbatim from the C++, where the vkCmdDraw is commented out next to it.

const std = @import("std");
const vk = @import("vulkan");

const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_scope = @import("../gpu_scope.zig");
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");
const imgui = @import("../../imgui.zig");
const pipeline_module = @import("../pipeline.zig");
const render_pass_helpers = @import("../render_pass_helpers.zig");
const shader_modules = @import("../shader_modules.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const PipelineFactory = @import("../pipeline_factory.zig").PipelineFactory;

/// GUIFormat in GuiPass.h. ImGui's vertex colours are sRGB-encoded bytes, so
/// the attachment carries the transfer function rather than the shader.
pub const gui_format: vk.Format = .r8g8b8a8_srgb;

fn createGuiPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
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

    const color_formats = [_]vk.Format{gui_format};

    var properties = pipeline_module.defaultGraphicsPipelineProperties(null);
    properties.blend_state.attachment_count = blend_attachment_states.len;
    properties.blend_state.p_attachments = &blend_attachment_states;
    properties.pipeline_layout = pipeline_layout;
    properties.pipeline_rendering.color_attachment_count = color_formats.len;
    properties.pipeline_rendering.p_color_attachment_formats = &color_formats;

    const dynamic_states = [_]vk.DynamicState{ .viewport, .scissor };

    return pipeline_module.createGraphicsPipeline(vkd, device, &shader_stages, &properties, &dynamic_states);
}

pub const GuiPassResources = struct {
    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,
    descriptor_set_layout: vk.DescriptorSetLayout,

    descriptor_set: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        pipeline_factory: *PipelineFactory,
    ) !GuiPassResources {
        // Nothing for now — the layout exists so the pipeline layout has a set
        // to reference, exactly as in the C++.
        const bindings = [_]vk.DescriptorSetLayoutBinding{};

        const descriptor_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &bindings, &.{});
        errdefer vkd.destroyDescriptorSetLayout(device, descriptor_set_layout, null);

        const pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &.{descriptor_set_layout}, &.{});
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = pipeline_layout,
            .pipeline_creation_function = &createGuiPipeline,
        });

        var sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &.{descriptor_set_layout}, &sets);

        return .{
            .pipeline_index = pipeline_index,
            .pipeline_layout = pipeline_layout,
            .descriptor_set_layout = descriptor_set_layout,
            .descriptor_set = sets[0],
        };
    }

    pub fn deinit(self: *GuiPassResources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.descriptor_set_layout, null);
    }
};

pub const GuiFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    output: fg.ResourceUsageHandle,
};

pub fn createFrameGraphRecord(builder: *Builder, gui_extent: vk.Extent2D) !GuiFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("GUI", false);

    const output = try builder.createTexture(
        pass_handle,
        "GUI SDR",
        gpu_texture_properties.defaultTextureProperties(
            gui_extent.width,
            gui_extent.height,
            gui_format,
            .{ .color_attachment = true, .sampled = true },
        ),
        fg.toTextureAccess(.{
            .stage_mask = .{ .color_attachment_output_bit = true },
            .access_mask = .{ .color_attachment_write_bit = true },
            .image_layout = .attachment_optimal,
        }),
        &.{},
    );

    return .{ .pass_handle = pass_handle, .output = output };
}

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: GuiFrameGraphRecord,
    resources: *const GuiPassResources,
) void {
    const scope = gpu_scope.begin(vkd, cmd_buffer, @src(), "GUI");
    defer scope.end(vkd, cmd_buffer);

    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const gui_buffer = helper.resources.getTexture(helper.frame_graph, record.output);

    const gui_extent = vk.Extent2D{
        .width = gui_buffer.properties.width,
        .height = gui_buffer.properties.height,
    };

    const pass_rect = render_pass_helpers.defaultRect(gui_extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(resources.pipeline_index));

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    var color_attachments = [_]vk.RenderingAttachmentInfo{
        pipeline_module.defaultRenderingAttachmentInfo(gui_buffer.default_view_handle, gui_buffer.image_layout),
    };
    color_attachments[0].load_op = .clear;
    color_attachments[0].clear_value = .{ .color = .{ .float_32 = .{ 0, 0, 0, 0 } } };

    const rendering_info = pipeline_module.defaultRenderingInfo(pass_rect, &color_attachments, null);

    vkd.cmdBeginRendering(cmd_buffer, &rendering_info);

    const sets = [_]vk.DescriptorSet{resources.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .graphics, resources.pipeline_layout, 0, &sets, &.{});

    // NOTE: the C++ has `vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);` commented
    // out right here — the bound pipeline is dead weight, ImGui binds its own.
    imgui.vulkanRenderDrawData(cmd_buffer);

    vkd.cmdEndRendering(cmd_buffer);
}
