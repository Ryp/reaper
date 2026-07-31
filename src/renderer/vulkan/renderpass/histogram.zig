// Port of src/renderer/vulkan/renderpass/HistogramPass.{h,cpp}
//
// Builds a luminance histogram of the forward pass's HDR output into a small
// storage buffer. Two passes: a clear that fills the buffer with zeroes, then a
// compute reduce that atomically accumulates into it.

const std = @import("std");
const vk = @import("vulkan");

const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_buffer = @import("../../buffer/gpu_buffer.zig");
const pipeline_module = @import("../pipeline.zig");
const shader_modules = @import("../shader_modules.zig");
const divRoundUp = @import("../compute_helper.zig").divRoundUp;

const hlsl = @import("../../hlsl/histogram/reduce_histogram.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const PipelineFactory = @import("../pipeline_factory.zig").PipelineFactory;
const SamplerResources = @import("../sampler_resources.zig").SamplerResources;

fn createHistogramPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("histogram/reduce_histogram.comp.spv"),
    );

    return pipeline_module.createComputePipeline(
        vkd,
        device,
        pipeline_layout,
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .compute_bit = true }, &module_create_info, null),
    );
}

pub const HistogramPassResources = struct {
    descriptor_set_layout: vk.DescriptorSetLayout,

    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,

    descriptor_set: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        pipeline_factory: *PipelineFactory,
    ) !HistogramPassResources {
        const bindings = [_]vk.DescriptorSetLayoutBinding{
            .{ .binding = 0, .descriptor_type = .sampler, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 1, .descriptor_type = .sampled_image, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 2, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
        };

        const descriptor_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &bindings, &.{});
        errdefer vkd.destroyDescriptorSetLayout(device, descriptor_set_layout, null);

        const push_constant_ranges = [_]vk.PushConstantRange{.{
            .stage_flags = .{ .compute_bit = true },
            .offset = 0,
            .size = @sizeOf(hlsl.ReduceHDRPassParams),
        }};

        const pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{descriptor_set_layout},
            &push_constant_ranges,
        );
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = pipeline_layout,
            .pipeline_creation_function = &createHistogramPipeline,
        });

        var sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &.{descriptor_set_layout}, &sets);

        return .{
            .descriptor_set_layout = descriptor_set_layout,
            .pipeline_index = pipeline_index,
            .pipeline_layout = pipeline_layout,
            .descriptor_set = sets[0],
        };
    }

    pub fn deinit(self: *HistogramPassResources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.descriptor_set_layout, null);
    }
};

// --------------------------------------------------------------------------
// Frame graph records
// --------------------------------------------------------------------------

pub const HistogramClearFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    histogram_buffer: fg.ResourceUsageHandle,
};

pub const HistogramFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    scene_hdr: fg.ResourceUsageHandle,
    histogram_buffer: fg.ResourceUsageHandle,
};

pub fn createClearFrameGraphRecord(builder: *Builder) !HistogramClearFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Histogram Clear", false);

    const histogram_buffer_properties = gpu_buffer.defaultBufferProperties(
        hlsl.HistogramRes,
        @sizeOf(u32),
        .{ .storage_buffer = true, .transfer_dst = true },
    );

    const histogram_buffer = try builder.createBuffer(
        pass_handle,
        "Histogram Buffer",
        histogram_buffer_properties,
        fg.toBufferAccess(.{
            .stage_mask = .{ .clear_bit = true },
            .access_mask = .{ .transfer_write_bit = true },
            .image_layout = .undefined,
        }),
        &.{},
    );

    return .{ .pass_handle = pass_handle, .histogram_buffer = histogram_buffer };
}

pub fn createFrameGraphRecord(
    builder: *Builder,
    histogram_clear: HistogramClearFrameGraphRecord,
    scene_hdr_usage_handle: fg.ResourceUsageHandle,
) !HistogramFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Histogram", false);

    const scene_hdr = try builder.readTexture(
        pass_handle,
        scene_hdr_usage_handle,
        fg.toTextureAccess(.{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_read_bit = true },
            .image_layout = .read_only_optimal,
        }),
        &.{},
    );

    const histogram_buffer = try builder.writeBuffer(
        pass_handle,
        histogram_clear.histogram_buffer,
        fg.toBufferAccess(.{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_write_bit = true, .shader_read_bit = true },
            .image_layout = .undefined,
        }),
        &.{},
    );

    return .{
        .pass_handle = pass_handle,
        .scene_hdr = scene_hdr,
        .histogram_buffer = histogram_buffer,
    };
}

// --------------------------------------------------------------------------
// Descriptor updates and recording
// --------------------------------------------------------------------------

pub fn updateDescriptorSet(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: HistogramFrameGraphRecord,
    resources: *const HistogramPassResources,
    sampler_resources: SamplerResources,
) void {
    const scene_hdr = frame_graph_resources.getTexture(framegraph, record.scene_hdr);
    const histogram_buffer = frame_graph_resources.getBuffer(framegraph, record.histogram_buffer);

    const set = resources.descriptor_set;

    write_helper.appendSampler(set, 0, sampler_resources.linear_clamp);
    write_helper.appendImage(set, 1, .sampled_image, scene_hdr.default_view_handle, scene_hdr.image_layout);
    write_helper.appendBuffer(set, 2, .storage_buffer, histogram_buffer.handle, 0, vk.WHOLE_SIZE);
}

pub fn recordClearCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    record: HistogramClearFrameGraphRecord,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const histogram_buffer = helper.resources.getBuffer(helper.frame_graph, record.histogram_buffer);

    vkd.cmdFillBuffer(
        cmd_buffer,
        histogram_buffer.handle,
        histogram_buffer.default_view.offset_bytes,
        histogram_buffer.default_view.size_bytes,
        0,
    );
}

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: HistogramFrameGraphRecord,
    resources: *const HistogramPassResources,
    render_extent: vk.Extent2D,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(resources.pipeline_index));

    const push_constants = hlsl.ReduceHDRPassParams{
        .extent_ts = .{ .x = render_extent.width, .y = render_extent.height },
        .extent_ts_inv = .{
            .x = 1.0 / @as(f32, @floatFromInt(render_extent.width)),
            .y = 1.0 / @as(f32, @floatFromInt(render_extent.height)),
        },
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        resources.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(hlsl.ReduceHDRPassParams),
        &push_constants,
    );

    const sets = [_]vk.DescriptorSet{resources.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.pipeline_layout, 0, &sets, &.{});

    // The shader reduces into a shared histogram of HistogramRes bins with one
    // thread group's worth of threads, so the bin count has to divide evenly.
    comptime std.debug.assert(hlsl.HistogramRes % (hlsl.HistogramThreadCountX * hlsl.HistogramThreadCountY) == 0);

    vkd.cmdDispatch(
        cmd_buffer,
        divRoundUp(render_extent.width, hlsl.HistogramThreadCountX * 2),
        divRoundUp(render_extent.height, hlsl.HistogramThreadCountY * 2),
        1,
    );
}
