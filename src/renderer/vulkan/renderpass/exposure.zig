// Port of src/renderer/vulkan/renderpass/ExposurePass.{h,cpp}
//
// Reduces the HDR scene down to a single average log2 luminance, in two steps:
// a wide reduce into a small R16G16_SFLOAT texture, then a "tail" pass that the
// last thread group to finish carries all the way down to one value.

const std = @import("std");
const vk = @import("vulkan");

const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_buffer = @import("../../buffer/gpu_buffer.zig");
const gpu_scope = @import("../gpu_scope.zig");
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");
const pipeline_module = @import("../pipeline.zig");
const shader_modules = @import("../shader_modules.zig");
const divRoundUp = @import("../compute_helper.zig").divRoundUp;

const hlsl = @import("../../hlsl/reduce_exposure.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const PipelineFactory = @import("../pipeline_factory.zig").PipelineFactory;
const SamplerResources = @import("../sampler_resources.zig").SamplerResources;

pub const exposure_texture_format: vk.Format = .r16g16_sfloat;

/// Both stages are subgroup-size sensitive: the shaders reduce with wave
/// intrinsics, so the driver is allowed to pick a size other than the pipeline
/// default. NOTE from the C++: this can be left off when
/// ENABLE_SHARED_FLOAT_ATOMICS is on.
fn createReducePipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    return createVaryingSubgroupComputePipeline(vkd, device, pipeline_layout, "reduce_exposure.comp.spv");
}

fn createReduceTailPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    return createVaryingSubgroupComputePipeline(vkd, device, pipeline_layout, "reduce_exposure_tail.comp.spv");
}

fn createVaryingSubgroupComputePipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
    comptime shader_name: []const u8,
) anyerror!vk.Pipeline {
    const module_create_info = pipeline_module.shaderModuleCreateInfo(shader_modules.get(shader_name));

    var shader_stage = pipeline_module.defaultPipelineShaderStageCreateInfo(
        .{ .compute_bit = true },
        &module_create_info,
        null,
    );
    shader_stage.flags = .{ .allow_varying_subgroup_size_bit = true };

    return pipeline_module.createComputePipeline(vkd, device, pipeline_layout, shader_stage);
}

pub const ExposurePassResources = struct {
    pub const Stage = struct {
        descriptor_set_layout: vk.DescriptorSetLayout,
        pipeline_layout: vk.PipelineLayout,
        pipeline_index: u32,

        descriptor_set: vk.DescriptorSet,
    };

    reduce: Stage,
    reduce_tail: Stage,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        pipeline_factory: *PipelineFactory,
    ) !ExposurePassResources {
        const reduce_bindings = [_]vk.DescriptorSetLayoutBinding{
            .{ .binding = 0, .descriptor_type = .sampler, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 1, .descriptor_type = .sampled_image, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 2, .descriptor_type = .storage_image, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
        };

        const reduce_tail_bindings = [_]vk.DescriptorSetLayoutBinding{
            .{ .binding = 0, .descriptor_type = .sampler, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 1, .descriptor_type = .sampled_image, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 2, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 3, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 4, .descriptor_type = .storage_image, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
        };

        const reduce_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &reduce_bindings, &.{});
        errdefer vkd.destroyDescriptorSetLayout(device, reduce_layout, null);

        const reduce_push_constant_ranges = [_]vk.PushConstantRange{.{
            .stage_flags = .{ .compute_bit = true },
            .offset = 0,
            .size = @sizeOf(hlsl.ReduceExposurePassParams),
        }};

        const reduce_pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{reduce_layout},
            &reduce_push_constant_ranges,
        );
        errdefer vkd.destroyPipelineLayout(device, reduce_pipeline_layout, null);

        const reduce_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = reduce_pipeline_layout,
            .pipeline_creation_function = &createReducePipeline,
        });

        const reduce_tail_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &reduce_tail_bindings, &.{});
        errdefer vkd.destroyDescriptorSetLayout(device, reduce_tail_layout, null);

        const reduce_tail_push_constant_ranges = [_]vk.PushConstantRange{.{
            .stage_flags = .{ .compute_bit = true },
            .offset = 0,
            .size = @sizeOf(hlsl.ReduceExposureTailPassParams),
        }};

        const reduce_tail_pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{reduce_tail_layout},
            &reduce_tail_push_constant_ranges,
        );
        errdefer vkd.destroyPipelineLayout(device, reduce_tail_pipeline_layout, null);

        const reduce_tail_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = reduce_tail_pipeline_layout,
            .pipeline_creation_function = &createReduceTailPipeline,
        });

        var reduce_sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &.{reduce_layout}, &reduce_sets);

        var reduce_tail_sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &.{reduce_tail_layout}, &reduce_tail_sets);

        return .{
            .reduce = .{
                .descriptor_set_layout = reduce_layout,
                .pipeline_layout = reduce_pipeline_layout,
                .pipeline_index = reduce_pipeline_index,
                .descriptor_set = reduce_sets[0],
            },
            .reduce_tail = .{
                .descriptor_set_layout = reduce_tail_layout,
                .pipeline_layout = reduce_tail_pipeline_layout,
                .pipeline_index = reduce_tail_pipeline_index,
                .descriptor_set = reduce_tail_sets[0],
            },
        };
    }

    pub fn deinit(self: *ExposurePassResources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipelineLayout(device, self.reduce.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.reduce.descriptor_set_layout, null);

        vkd.destroyPipelineLayout(device, self.reduce_tail.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.reduce_tail.descriptor_set_layout, null);
    }
};

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const ExposureFrameGraphRecord = struct {
    pub const Reduce = struct {
        pass_handle: fg.RenderPassHandle,
        scene_hdr: fg.ResourceUsageHandle,
        exposure_texture: fg.ResourceUsageHandle,
        tail_counter: fg.ResourceUsageHandle,
    };

    pub const ReduceTail = struct {
        pass_handle: fg.RenderPassHandle,
        exposure_texture: fg.ResourceUsageHandle,
        average_exposure: fg.ResourceUsageHandle,
        tail_counter: fg.ResourceUsageHandle,
        exposure_texture_tail: fg.ResourceUsageHandle,
    };

    reduce: Reduce,
    reduce_tail: ReduceTail,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    scene_hdr_usage_handle: fg.ResourceUsageHandle,
    render_extent: vk.Extent2D,
) !ExposureFrameGraphRecord {
    const reduced_width = divRoundUp(render_extent.width, hlsl.ExposureThreadCountX * 2);
    const reduced_height = divRoundUp(render_extent.height, hlsl.ExposureThreadCountY * 2);

    const reduce: ExposureFrameGraphRecord.Reduce = blk: {
        const pass_handle = try builder.createRenderPass("Exposure", false);

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

        const exposure_texture = try builder.createTexture(
            pass_handle,
            "Average Log2 Luminance Texture",
            gpu_texture_properties.defaultTextureProperties(
                reduced_width,
                reduced_height,
                exposure_texture_format,
                .{ .storage = true, .sampled = true },
            ),
            fg.toTextureAccess(.{
                .stage_mask = .{ .compute_shader_bit = true },
                .access_mask = .{ .shader_write_bit = true },
                .image_layout = .general,
            }),
            &.{},
        );

        const tail_counter = try builder.createBuffer(
            pass_handle,
            "Exposure Tail Counter",
            gpu_buffer.defaultBufferProperties(1, @sizeOf(u32), .{ .transfer_dst = true, .storage_buffer = true }),
            fg.toBufferAccess(.{
                .stage_mask = .{ .clear_bit = true },
                .access_mask = .{ .transfer_write_bit = true },
                .image_layout = .undefined,
            }),
            &.{},
        );

        break :blk .{
            .pass_handle = pass_handle,
            .scene_hdr = scene_hdr,
            .exposure_texture = exposure_texture,
            .tail_counter = tail_counter,
        };
    };

    const reduce_tail: ExposureFrameGraphRecord.ReduceTail = blk: {
        const pass_handle = try builder.createRenderPass("Exposure Reduce Tail", false);

        const exposure_texture = try builder.readTexture(
            pass_handle,
            reduce.exposure_texture,
            fg.toTextureAccess(.{
                .stage_mask = .{ .compute_shader_bit = true },
                .access_mask = .{ .shader_read_bit = true },
                .image_layout = .read_only_optimal,
            }),
            &.{},
        );

        const average_exposure = try builder.createBuffer(
            pass_handle,
            "Average Log2 Luminance",
            gpu_buffer.defaultBufferProperties(1, @sizeOf(u32), .{ .storage_buffer = true }),
            fg.toBufferAccess(.{
                .stage_mask = .{ .compute_shader_bit = true },
                .access_mask = .{ .shader_write_bit = true },
                .image_layout = .undefined,
            }),
            &.{},
        );

        const tail_counter = try builder.writeBuffer(
            pass_handle,
            reduce.tail_counter,
            fg.toBufferAccess(.{
                .stage_mask = .{ .compute_shader_bit = true },
                .access_mask = .{ .shader_read_bit = true, .shader_write_bit = true },
                .image_layout = .undefined,
            }),
            &.{},
        );

        const reduced_tail_width = divRoundUp(reduced_width, hlsl.ExposureThreadCountX * 2);
        const reduced_tail_height = divRoundUp(reduced_height, hlsl.ExposureThreadCountY * 2);

        const exposure_texture_tail = try builder.createTexture(
            pass_handle,
            "Average Log2 Luminance Texture Tail",
            gpu_texture_properties.defaultTextureProperties(
                reduced_tail_width,
                reduced_tail_height,
                exposure_texture_format,
                .{ .storage = true, .sampled = true },
            ),
            fg.toTextureAccess(.{
                .stage_mask = .{ .compute_shader_bit = true },
                .access_mask = .{ .shader_read_bit = true, .shader_write_bit = true },
                .image_layout = .general,
            }),
            &.{},
        );

        break :blk .{
            .pass_handle = pass_handle,
            .exposure_texture = exposure_texture,
            .average_exposure = average_exposure,
            .tail_counter = tail_counter,
            .exposure_texture_tail = exposure_texture_tail,
        };
    };

    return .{ .reduce = reduce, .reduce_tail = reduce_tail };
}

// --------------------------------------------------------------------------
// Descriptor updates and recording
// --------------------------------------------------------------------------

pub fn updateDescriptorSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: ExposureFrameGraphRecord,
    resources: *const ExposurePassResources,
    sampler_resources: SamplerResources,
) void {
    {
        const scene_hdr = frame_graph_resources.getTexture(framegraph, record.reduce.scene_hdr);
        const exposure_texture = frame_graph_resources.getTexture(framegraph, record.reduce.exposure_texture);

        const set = resources.reduce.descriptor_set;

        write_helper.appendSampler(set, 0, sampler_resources.linear_clamp);
        write_helper.appendImage(set, 1, .sampled_image, scene_hdr.default_view_handle, scene_hdr.image_layout);
        write_helper.appendImage(set, 2, .storage_image, exposure_texture.default_view_handle, exposure_texture.image_layout);
    }

    {
        const exposure_texture = frame_graph_resources.getTexture(framegraph, record.reduce_tail.exposure_texture);
        const average_exposure = frame_graph_resources.getBuffer(framegraph, record.reduce_tail.average_exposure);
        const counter = frame_graph_resources.getBuffer(framegraph, record.reduce_tail.tail_counter);
        const exposure_texture_tail = frame_graph_resources.getTexture(framegraph, record.reduce_tail.exposure_texture_tail);

        const set = resources.reduce_tail.descriptor_set;

        write_helper.appendSampler(set, 0, sampler_resources.linear_clamp);
        write_helper.appendImage(set, 1, .sampled_image, exposure_texture.default_view_handle, exposure_texture.image_layout);
        write_helper.appendBuffer(set, 2, .storage_buffer, average_exposure.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(set, 3, .storage_buffer, counter.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendImage(set, 4, .storage_image, exposure_texture_tail.default_view_handle, exposure_texture_tail.image_layout);
    }
}

fn recordReduceCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: ExposureFrameGraphRecord.Reduce,
    resources: *const ExposurePassResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    // NOTE: we could create it and clear it once at startup on the CPU, then
    // reset it with compute at the end of each pass, like AMD's SPD code does.
    const tail_counter = helper.resources.getBuffer(helper.frame_graph, record.tail_counter);
    vkd.cmdFillBuffer(cmd_buffer, tail_counter.handle, 0, vk.WHOLE_SIZE, 0);

    const scene_hdr = helper.resources.getTexture(helper.frame_graph, record.scene_hdr);

    // FIXME (from the C++): the extent comes from the texture, not the pass.
    const scene_hdr_extent = vk.Extent2D{
        .width = scene_hdr.properties.width,
        .height = scene_hdr.properties.height,
    };

    const pipe = resources.reduce;

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(pipe.pipeline_index));

    const push_constants = hlsl.ReduceExposurePassParams{
        .extent_ts = .{ .x = scene_hdr_extent.width, .y = scene_hdr_extent.height },
        .extent_ts_inv = .{
            .x = 1.0 / @as(f32, @floatFromInt(scene_hdr_extent.width)),
            .y = 1.0 / @as(f32, @floatFromInt(scene_hdr_extent.height)),
        },
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        pipe.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(hlsl.ReduceExposurePassParams),
        &push_constants,
    );

    const sets = [_]vk.DescriptorSet{pipe.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, pipe.pipeline_layout, 0, &sets, &.{});

    vkd.cmdDispatch(
        cmd_buffer,
        divRoundUp(scene_hdr_extent.width, hlsl.ExposureThreadCountX * 2),
        divRoundUp(scene_hdr_extent.height, hlsl.ExposureThreadCountY * 2),
        1,
    );
}

fn recordReduceTailCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: ExposureFrameGraphRecord.ReduceTail,
    resources: *const ExposurePassResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const exposure_texture = helper.resources.getTexture(helper.frame_graph, record.exposure_texture);

    const exposure_extent = vk.Extent2D{
        .width = exposure_texture.properties.width,
        .height = exposure_texture.properties.height,
    };

    const pipe = resources.reduce_tail;

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(pipe.pipeline_index));

    const group_size_x = divRoundUp(exposure_extent.width, hlsl.ExposureThreadCountX * 2);
    const group_size_y = divRoundUp(exposure_extent.height, hlsl.ExposureThreadCountY * 2);
    const group_count = group_size_x * group_size_y;

    const push_constants = hlsl.ReduceExposureTailPassParams{
        .extent_ts = .{ .x = exposure_extent.width, .y = exposure_extent.height },
        .extent_ts_inv = .{
            .x = 1.0 / @as(f32, @floatFromInt(exposure_extent.width)),
            .y = 1.0 / @as(f32, @floatFromInt(exposure_extent.height)),
        },
        .tail_extent_ts = .{ .x = group_size_x, .y = group_size_y },
        .last_thread_group_index = group_count - 1,
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        pipe.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(hlsl.ReduceExposureTailPassParams),
        &push_constants,
    );

    const sets = [_]vk.DescriptorSet{pipe.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, pipe.pipeline_layout, 0, &sets, &.{});

    vkd.cmdDispatch(cmd_buffer, group_size_x, group_size_y, 1);
}

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: ExposureFrameGraphRecord,
    resources: *const ExposurePassResources,
) void {
    const scope = gpu_scope.begin(vkd, cmd_buffer, @src(), "Compute Exposure");
    defer scope.end(vkd, cmd_buffer);

    recordReduceCommandBuffer(vkd, cmd_buffer, helper, pipeline_factory, record.reduce, resources);
    recordReduceTailCommandBuffer(vkd, cmd_buffer, helper, pipeline_factory, record.reduce_tail, resources);
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "the tail reduce shrinks the reduced extent by another full step" {
    // The tail pass reads what the first reduce wrote, so its dispatch has to
    // be derived from the *reduced* extent, not the render extent — getting
    // this wrong silently averages over the wrong number of texels.
    const render = vk.Extent2D{ .width = 1920, .height = 1080 };

    const reduced_width = divRoundUp(render.width, hlsl.ExposureThreadCountX * 2);
    const reduced_height = divRoundUp(render.height, hlsl.ExposureThreadCountY * 2);

    try testing.expectEqual(@as(u32, 120), reduced_width);
    try testing.expectEqual(@as(u32, 68), reduced_height);

    try testing.expectEqual(@as(u32, 8), divRoundUp(reduced_width, hlsl.ExposureThreadCountX * 2));
    try testing.expectEqual(@as(u32, 5), divRoundUp(reduced_height, hlsl.ExposureThreadCountY * 2));
}

test "a one-pixel viewport still dispatches one group per stage" {
    // divRoundUp must never produce a zero dispatch: a zero group count means
    // the tail pass's last_thread_group_index underflows.
    const reduced_width = divRoundUp(@as(u32, 1), hlsl.ExposureThreadCountX * 2);
    const reduced_height = divRoundUp(@as(u32, 1), hlsl.ExposureThreadCountY * 2);

    try testing.expectEqual(@as(u32, 1), reduced_width);
    try testing.expectEqual(@as(u32, 1), reduced_height);

    const group_count = divRoundUp(reduced_width, hlsl.ExposureThreadCountX * 2) *
        divRoundUp(reduced_height, hlsl.ExposureThreadCountY * 2);

    try testing.expectEqual(@as(u32, 1), group_count);
    try testing.expectEqual(@as(u32, 0), group_count - 1);
}
