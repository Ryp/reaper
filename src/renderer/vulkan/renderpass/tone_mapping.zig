// Port of src/renderer/vulkan/renderpass/ToneMappingPass.{h,cpp}
//
// Bakes the tone mapping curve into a 1D LUT once per frame. Self-contained:
// no scene input, only two push constants, which makes it the first real
// ported pass whose output can be checked on its own.

const std = @import("std");
const vk = @import("vulkan");

const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_scope = @import("../gpu_scope.zig");
const hlsl = @import("../../hlsl/tone_mapping_bake_lut.zig");
const pipeline_module = @import("../pipeline.zig");
const shader_modules = @import("../shader_modules.zig");
const divRoundUp = @import("../compute_helper.zig").divRoundUp;

const Builder = @import("../../graph/builder.zig").Builder;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const PipelineFactory = @import("../pipeline_factory.zig").PipelineFactory;
const barrier_module = @import("../barrier.zig");
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");

pub const lut_format: vk.Format = .r32_sfloat;

const lut_access = barrier_module.GPUTextureAccess{
    .stage_mask = .{ .compute_shader_bit = true },
    .access_mask = .{ .shader_write_bit = true },
    .image_layout = .general,
};

fn createBakePipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("tone_mapping_bake_lut.comp.spv"),
    );

    return pipeline_module.createComputePipeline(
        vkd,
        device,
        pipeline_layout,
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .compute_bit = true }, &module_create_info, null),
    );
}

pub const ToneMapPassResources = struct {
    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,
    descriptor_set_layout: vk.DescriptorSetLayout,

    descriptor_set: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        pipeline_factory: *PipelineFactory,
    ) !ToneMapPassResources {
        const bindings = [_]vk.DescriptorSetLayoutBinding{.{
            .binding = 0,
            .descriptor_type = .storage_image,
            .descriptor_count = 1,
            .stage_flags = .{ .compute_bit = true },
            .p_immutable_samplers = null,
        }};

        const descriptor_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &bindings, &.{});
        errdefer vkd.destroyDescriptorSetLayout(device, descriptor_set_layout, null);

        const push_constant_ranges = [_]vk.PushConstantRange{.{
            .stage_flags = .{ .compute_bit = true },
            .offset = 0,
            .size = @sizeOf(hlsl.ToneMappingBakeLUT_Consts),
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
            .pipeline_creation_function = &createBakePipeline,
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

    pub fn deinit(self: *ToneMapPassResources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.descriptor_set_layout, null);
    }
};

pub const ToneMapPassRecord = struct {
    pass_handle: fg.RenderPassHandle,
    tone_map_lut: fg.ResourceUsageHandle,
};

/// `has_side_effects` is false in the C++, because the swapchain pass consumes
/// the LUT and therefore keeps this pass reachable in the DAG. Until that pass
/// exists, callers have to pass true or the graph prunes the bake entirely.
pub fn createFrameGraphRecord(builder: *Builder, has_side_effects: bool) !ToneMapPassRecord {
    const pass_handle = try builder.createRenderPass("Tone Map Bake LUT", has_side_effects);

    var properties = gpu_texture_properties.defaultTextureProperties(
        hlsl.ToneMappingBakeLUT_Res,
        1,
        lut_format,
        .{ .storage = true, .sampled = true },
    );
    properties.type = .tex_1d;

    const tone_map_lut = try builder.createTexture(
        pass_handle,
        "Tone Map LUT",
        properties,
        lut_access,
        &.{},
    );

    return .{ .pass_handle = pass_handle, .tone_map_lut = tone_map_lut };
}

pub fn updateDescriptorSet(
    write_helper: *descriptor_set.DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: ToneMapPassRecord,
    resources: *const ToneMapPassResources,
) void {
    const tone_map_lut = frame_graph_resources.getTexture(framegraph, record.tone_map_lut);

    write_helper.appendImage(
        resources.descriptor_set,
        0,
        .storage_image,
        tone_map_lut.default_view_handle,
        tone_map_lut.image_layout,
    );
}

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: ToneMapPassRecord,
    resources: *const ToneMapPassResources,
    tonemap_min_nits: f32,
    tonemap_max_nits: f32,
) void {
    const scope = gpu_scope.begin(vkd, cmd_buffer, @src(), "Tone Map Bake LUT");
    defer scope.end(vkd, cmd_buffer);

    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(resources.pipeline_index));

    const sets = [_]vk.DescriptorSet{resources.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.pipeline_layout, 0, &sets, &.{});

    const push_constants = hlsl.ToneMappingBakeLUT_Consts{
        .min_nits = tonemap_min_nits,
        .max_nits = tonemap_max_nits,
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        resources.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(hlsl.ToneMappingBakeLUT_Consts),
        &push_constants,
    );

    vkd.cmdDispatch(
        cmd_buffer,
        divRoundUp(hlsl.ToneMappingBakeLUT_Res, hlsl.ToneMappingBakeLUT_ThreadCount),
        1,
        1,
    );
}
