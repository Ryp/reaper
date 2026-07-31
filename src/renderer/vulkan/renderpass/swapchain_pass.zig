// Port of src/renderer/vulkan/renderpass/SwapchainPass.{h,cpp}
//
// The final composite: tone maps the HDR scene through the baked LUT and writes
// the swapchain image. The colour space, transfer function and dynamic range
// are specialization constants baked from the swapchain format, so the pipeline
// has to be rebuilt whenever the swapchain is reconfigured — which is why it
// lives outside the pipeline factory.
//
// The shader wants seven inputs. Four of them come from passes that do not
// exist yet (tiled lighting in M6, GUI/histogram/exposure in M7), so those are
// fed placeholder resources declared in the graph. They are real graph
// resources rather than unbound descriptors: the fragment shader samples all of
// them unconditionally, so leaving them unbound would be undefined behaviour
// rather than simply "off".

const std = @import("std");
const vk = @import("vulkan");

const buffer_module = @import("../buffer.zig");
const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_buffer = @import("../../buffer/gpu_buffer.zig");
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");
const pipeline_module = @import("../pipeline.zig");
const render_pass_helpers = @import("../render_pass_helpers.zig");
const shader_modules = @import("../shader_modules.zig");

const hlsl = @import("../../hlsl/swapchain_write.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const SamplerResources = @import("../sampler_resources.zig").SamplerResources;
const Swapchain = @import("../Swapchain.zig");

// --------------------------------------------------------------------------
// Specialization constants
// --------------------------------------------------------------------------

const SpecConstants = extern struct {
    color_space_index: u32,
    transfer_function_index: u32,
    dynamic_range: u32,
};

fn getColorSpaceIndex(color_space: Swapchain.ColorSpace) u32 {
    return switch (color_space) {
        .sRGB => hlsl.COLOR_SPACE_SRGB,
        .Rec709 => hlsl.COLOR_SPACE_REC709,
        .DisplayP3 => hlsl.COLOR_SPACE_DISPLAY_P3,
        .Rec2020 => hlsl.COLOR_SPACE_REC2020,
        .Unknown => unreachable,
    };
}

fn getTransferFunctionIndex(transfer_function: Swapchain.TransferFunction) u32 {
    return switch (transfer_function) {
        .Linear => hlsl.TRANSFER_FUNC_LINEAR,
        .sRGB => hlsl.TRANSFER_FUNC_SRGB,
        .Rec709 => hlsl.TRANSFER_FUNC_REC709,
        .PQ => hlsl.TRANSFER_FUNC_PQ,
        .scRGB_Windows => hlsl.TRANSFER_FUNC_WINDOWS_SCRGB,
        .Unknown => unreachable,
    };
}

// --------------------------------------------------------------------------
// Resources
// --------------------------------------------------------------------------

pub const SwapchainPassResources = struct {
    descriptor_set_layout: vk.DescriptorSetLayout,
    pipeline_layout: vk.PipelineLayout,
    pipeline: vk.Pipeline,
    descriptor_set: vk.DescriptorSet,

    /// The swapchain format the pipeline's spec constants were baked from.
    swapchain_format: Swapchain.SwapchainFormat,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        swapchain_format: Swapchain.SwapchainFormat,
    ) !SwapchainPassResources {
        const bindings = [_]vk.DescriptorSetLayoutBinding{
            .{ .binding = 0, .descriptor_type = .sampler, .descriptor_count = 1, .stage_flags = .{ .fragment_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 1, .descriptor_type = .sampled_image, .descriptor_count = 1, .stage_flags = .{ .fragment_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 2, .descriptor_type = .sampled_image, .descriptor_count = 1, .stage_flags = .{ .fragment_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 3, .descriptor_type = .sampled_image, .descriptor_count = 1, .stage_flags = .{ .fragment_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 4, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .fragment_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 5, .descriptor_type = .sampled_image, .descriptor_count = 1, .stage_flags = .{ .fragment_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 6, .descriptor_type = .sampled_image, .descriptor_count = 1, .stage_flags = .{ .fragment_bit = true }, .p_immutable_samplers = null },
        };

        const descriptor_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &bindings, &.{});
        errdefer vkd.destroyDescriptorSetLayout(device, descriptor_set_layout, null);

        const push_constant_ranges = [_]vk.PushConstantRange{.{
            .stage_flags = .{ .fragment_bit = true },
            .offset = 0,
            .size = @sizeOf(hlsl.SwapchainWriteParams),
        }};

        const pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{descriptor_set_layout},
            &push_constant_ranges,
        );
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const pipeline = try createPipeline(vkd, device, pipeline_layout, swapchain_format);
        errdefer vkd.destroyPipeline(device, pipeline, null);

        var sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &.{descriptor_set_layout}, &sets);

        return .{
            .descriptor_set_layout = descriptor_set_layout,
            .pipeline_layout = pipeline_layout,
            .pipeline = pipeline,
            .descriptor_set = sets[0],
            .swapchain_format = swapchain_format,
        };
    }

    pub fn deinit(self: *SwapchainPassResources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipeline(device, self.pipeline, null);
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.descriptor_set_layout, null);
    }

    /// Mirrors reload_swapchain_pipeline(). The caller must have waited for the
    /// device to go idle.
    pub fn reconfigure(
        self: *SwapchainPassResources,
        vkd: anytype,
        device: vk.Device,
        swapchain_format: Swapchain.SwapchainFormat,
    ) !void {
        if (std.meta.eql(self.swapchain_format, swapchain_format)) return;

        const pipeline = try createPipeline(vkd, device, self.pipeline_layout, swapchain_format);

        vkd.destroyPipeline(device, self.pipeline, null);
        self.pipeline = pipeline;
        self.swapchain_format = swapchain_format;
    }
};

fn createPipeline(
    vkd: anytype,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
    swapchain_format: Swapchain.SwapchainFormat,
) !vk.Pipeline {
    const spec_constants = SpecConstants{
        .color_space_index = getColorSpaceIndex(swapchain_format.color_space),
        .transfer_function_index = getTransferFunctionIndex(swapchain_format.transfer_function),
        .dynamic_range = if (swapchain_format.is_hdr) hlsl.DYNAMIC_RANGE_HDR else hlsl.DYNAMIC_RANGE_SDR,
    };

    const spec_entries = [_]vk.SpecializationMapEntry{
        .{ .constant_id = 0, .offset = @offsetOf(SpecConstants, "color_space_index"), .size = @sizeOf(u32) },
        .{ .constant_id = 1, .offset = @offsetOf(SpecConstants, "transfer_function_index"), .size = @sizeOf(u32) },
        .{ .constant_id = 2, .offset = @offsetOf(SpecConstants, "dynamic_range"), .size = @sizeOf(u32) },
    };

    const specialization = vk.SpecializationInfo{
        .map_entry_count = spec_entries.len,
        .p_map_entries = &spec_entries,
        .data_size = @sizeOf(SpecConstants),
        .p_data = &spec_constants,
    };

    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("fullscreen_triangle.vert.spv"),
    );
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("swapchain_write.frag.spv"),
    );

    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
        pipeline_module.defaultPipelineShaderStageCreateInfo(
            .{ .fragment_bit = true },
            &module_create_info_frag,
            &specialization,
        ),
    };

    const blend_attachment_states = [_]vk.PipelineColorBlendAttachmentState{
        pipeline_module.defaultPipelineColorBlendAttachmentState(),
    };

    const color_formats = [_]vk.Format{swapchain_format.vk_view_format};

    var properties = pipeline_module.defaultGraphicsPipelineProperties(null);
    properties.blend_state.attachment_count = blend_attachment_states.len;
    properties.blend_state.p_attachments = &blend_attachment_states;
    properties.pipeline_layout = pipeline_layout;
    properties.pipeline_rendering.color_attachment_count = color_formats.len;
    properties.pipeline_rendering.p_color_attachment_formats = &color_formats;

    const dynamic_states = [_]vk.DynamicState{ .viewport, .scissor };

    return pipeline_module.createGraphicsPipeline(vkd, device, &shader_stages, &properties, &dynamic_states);
}

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const SwapchainFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    scene_hdr: fg.ResourceUsageHandle,
    lighting_result: fg.ResourceUsageHandle,
    gui: fg.ResourceUsageHandle,
    histogram: fg.ResourceUsageHandle,
    average_exposure: fg.ResourceUsageHandle,
    tone_map_lut: fg.ResourceUsageHandle,
    tile_debug: fg.ResourceUsageHandle,
};

const read_sampled = fg.GPUResourceAccess{
    .stage_mask = .{ .fragment_shader_bit = true },
    .access_mask = .{ .shader_read_bit = true },
    .image_layout = .read_only_optimal,
};

const read_storage = fg.GPUResourceAccess{
    .stage_mask = .{ .fragment_shader_bit = true },
    .access_mask = .{ .shader_read_bit = true },
    .image_layout = .undefined,
};

/// What the composite samples besides the forward pass's HDR target. Every one
/// of these now comes from a real pass.
pub const SwapchainInputs = struct {
    lighting_result: fg.ResourceUsageHandle,
    gui: fg.ResourceUsageHandle,
    histogram: fg.ResourceUsageHandle,
    average_exposure: fg.ResourceUsageHandle,
    tile_debug: fg.ResourceUsageHandle,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    scene_hdr_usage_handle: fg.ResourceUsageHandle,
    inputs: SwapchainInputs,
    tone_map_lut: fg.ResourceUsageHandle,
) !SwapchainFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Swapchain", true);

    return .{
        .pass_handle = pass_handle,
        .scene_hdr = try builder.readTexture(pass_handle, scene_hdr_usage_handle, fg.toTextureAccess(read_sampled), &.{}),
        .lighting_result = try builder.readTexture(pass_handle, inputs.lighting_result, fg.toTextureAccess(read_sampled), &.{}),
        .gui = try builder.readTexture(pass_handle, inputs.gui, fg.toTextureAccess(read_sampled), &.{}),
        // FIXME just to hook the pass to the render graph
        .histogram = try builder.readBuffer(pass_handle, inputs.histogram, fg.toBufferAccess(read_storage), &.{}),
        .average_exposure = try builder.readBuffer(pass_handle, inputs.average_exposure, fg.toBufferAccess(read_storage), &.{}),
        .tone_map_lut = try builder.readTexture(pass_handle, tone_map_lut, fg.toTextureAccess(read_sampled), &.{}),
        .tile_debug = try builder.readTexture(pass_handle, inputs.tile_debug, fg.toTextureAccess(read_sampled), &.{}),
    };
}

// --------------------------------------------------------------------------
// Descriptor updates and recording
// --------------------------------------------------------------------------

pub fn updateDescriptorSet(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: SwapchainFrameGraphRecord,
    resources: *const SwapchainPassResources,
    sampler_resources: SamplerResources,
) void {
    const hdr_scene_texture = frame_graph_resources.getTexture(framegraph, record.scene_hdr);
    const lighting_texture = frame_graph_resources.getTexture(framegraph, record.lighting_result);
    const gui_texture = frame_graph_resources.getTexture(framegraph, record.gui);
    const average_exposure = frame_graph_resources.getBuffer(framegraph, record.average_exposure);
    const tile_debug_texture = frame_graph_resources.getTexture(framegraph, record.tile_debug);
    const tone_map_lut = frame_graph_resources.getTexture(framegraph, record.tone_map_lut);

    const set = resources.descriptor_set;

    write_helper.appendSampler(set, 0, sampler_resources.linear_black_border);
    write_helper.appendImage(set, 1, .sampled_image, hdr_scene_texture.default_view_handle, hdr_scene_texture.image_layout);
    write_helper.appendImage(set, 2, .sampled_image, lighting_texture.default_view_handle, lighting_texture.image_layout);
    write_helper.appendImage(set, 3, .sampled_image, gui_texture.default_view_handle, gui_texture.image_layout);
    write_helper.appendBuffer(set, 4, .storage_buffer, average_exposure.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendImage(set, 5, .sampled_image, tone_map_lut.default_view_handle, tone_map_lut.image_layout);
    write_helper.appendImage(set, 6, .sampled_image, tile_debug_texture.default_view_handle, tile_debug_texture.image_layout);
}

pub const CompositeParams = struct {
    exposure_compensation_stops: f32,
    tonemap_min_nits: f32,
    tonemap_max_nits: f32,
    sdr_ui_max_brightness_nits: f32,
    sdr_peak_brightness_nits: f32,
};

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    record: SwapchainFrameGraphRecord,
    resources: *const SwapchainPassResources,
    swapchain_buffer_view: vk.ImageView,
    swapchain_extent: vk.Extent2D,
    params: CompositeParams,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const pass_rect = render_pass_helpers.defaultRect(swapchain_extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, resources.pipeline);

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    var color_attachments = [_]vk.RenderingAttachmentInfo{
        pipeline_module.defaultRenderingAttachmentInfo(swapchain_buffer_view, .attachment_optimal),
    };
    color_attachments[0].load_op = .dont_care;

    const rendering_info = pipeline_module.defaultRenderingInfo(pass_rect, &color_attachments, null);

    vkd.cmdBeginRendering(cmd_buffer, &rendering_info);

    const push_constants = hlsl.SwapchainWriteParams{
        .exposure_compensation = std.math.exp2(params.exposure_compensation_stops),
        .tonemap_min_nits = params.tonemap_min_nits,
        .tonemap_max_nits = params.tonemap_max_nits,
        .sdr_ui_max_brightness_nits = params.sdr_ui_max_brightness_nits,
        .sdr_peak_brightness_nits = params.sdr_peak_brightness_nits,
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        resources.pipeline_layout,
        .{ .fragment_bit = true },
        0,
        @sizeOf(hlsl.SwapchainWriteParams),
        &push_constants,
    );

    const sets = [_]vk.DescriptorSet{resources.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .graphics, resources.pipeline_layout, 0, &sets, &.{});

    vkd.cmdDraw(cmd_buffer, 3, 1, 0, 0);

    vkd.cmdEndRendering(cmd_buffer);
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "spec constant offsets match the shader's constant ids" {
    // The shader declares VK_CONSTANT(0..2) in this order; a reordering here
    // would silently tone map with the wrong transfer function.
    try testing.expectEqual(@as(usize, 0), @offsetOf(SpecConstants, "color_space_index"));
    try testing.expectEqual(@as(usize, 4), @offsetOf(SpecConstants, "transfer_function_index"));
    try testing.expectEqual(@as(usize, 8), @offsetOf(SpecConstants, "dynamic_range"));
    try testing.expectEqual(@as(usize, 12), @sizeOf(SpecConstants));
}

test "swapchain format maps to the shader's enums" {
    try testing.expectEqual(hlsl.COLOR_SPACE_SRGB, getColorSpaceIndex(.sRGB));
    try testing.expectEqual(hlsl.COLOR_SPACE_REC2020, getColorSpaceIndex(.Rec2020));

    // An sRGB swapchain has its EOTF applied by the image view, so the shader
    // is told the transfer function is linear — see configureVulkanWmSwapchain.
    try testing.expectEqual(hlsl.TRANSFER_FUNC_LINEAR, getTransferFunctionIndex(.Linear));
    try testing.expectEqual(hlsl.TRANSFER_FUNC_PQ, getTransferFunctionIndex(.PQ));
}
