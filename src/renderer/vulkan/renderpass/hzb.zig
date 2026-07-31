// Port of src/renderer/vulkan/renderpass/HZBPass.{h,cpp}
//
// One compute dispatch reduces the scene depth into a 4-mip hierarchical Z
// buffer. The tiled lighting raster pass tests light volumes against it, so the
// HZB is sized to the tile grid rather than to the depth buffer.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("../barrier.zig");
const compute_helper = @import("../compute_helper.zig");
const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_scope = @import("../gpu_scope.zig");
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");
const gpu_texture_view = @import("../../texture/gpu_texture_view.zig");
const pipeline_module = @import("../pipeline.zig");
const pipeline_factory_module = @import("../pipeline_factory.zig");
const shader_modules = @import("../shader_modules.zig");
const tiled_lighting_common = @import("tiled_lighting_common.zig");

const hlsl_hzb = @import("../../hlsl/hzb_reduce.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const PipelineFactory = pipeline_factory_module.PipelineFactory;
const SamplerResources = @import("../sampler_resources.zig").SamplerResources;

/// FIXME hardcoded in the C++ too.
const hzb_mip_count: u32 = 4;

const hzb_format: vk.Format = .r16g16_unorm;

// --------------------------------------------------------------------------
// Descriptor bindings
// --------------------------------------------------------------------------

const Binding = struct {
    const linear_clamp_sampler = 0;
    const scene_depth = 1;
    const hzb_mips = 2;

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .sampler, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 1, .count = 1, .type = .sampled_image, .stage_mask = .{ .compute_bit = true } },
        .{
            .slot = 2,
            .count = hlsl_hzb.HZBMaxMipCount,
            .type = .storage_image,
            .stage_mask = .{ .compute_bit = true },
        },
    };
};

// --------------------------------------------------------------------------
// Pipeline
// --------------------------------------------------------------------------

fn createHzbPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info = pipeline_module.shaderModuleCreateInfo(shader_modules.get("hzb_reduce.comp.spv"));

    // The reduction is wave-width aware, so the driver is allowed to pick a
    // subgroup size other than the pipeline default.
    var shader_stage = pipeline_module.defaultPipelineShaderStageCreateInfo(
        .{ .compute_bit = true },
        &module_create_info,
        null,
    );
    shader_stage.flags = .{ .allow_varying_subgroup_size_bit = true };

    return pipeline_module.createComputePipeline(vkd, device, pipeline_layout, shader_stage);
}

// --------------------------------------------------------------------------
// Resources
// --------------------------------------------------------------------------

pub const HZBPassResources = struct {
    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,
    desc_set_layout: vk.DescriptorSetLayout,

    descriptor_set: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        pipeline_factory: *PipelineFactory,
    ) !HZBPassResources {
        var layout_bindings: [Binding.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&layout_bindings, &Binding.bindings);

        // The mip array is sized for HZBMaxMipCount but only hzb_mip_count of
        // them are written.
        var binding_flags = [_]vk.DescriptorBindingFlags{.{}} ** Binding.bindings.len;
        binding_flags[binding_flags.len - 1] = .{ .partially_bound_bit = true };

        const desc_set_layout = try pipeline_module.createDescriptorSetLayout(
            vkd,
            device,
            &layout_bindings,
            &binding_flags,
        );
        errdefer vkd.destroyDescriptorSetLayout(device, desc_set_layout, null);

        const push_constant_ranges = [_]vk.PushConstantRange{.{
            .stage_flags = .{ .compute_bit = true },
            .offset = 0,
            .size = @sizeOf(hlsl_hzb.HZBReducePushConstants),
        }};

        const pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{desc_set_layout},
            &push_constant_ranges,
        );
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = pipeline_layout,
            .pipeline_creation_function = &createHzbPipeline,
        });

        var descriptor_sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &.{desc_set_layout}, &descriptor_sets);

        return .{
            .pipeline_index = pipeline_index,
            .pipeline_layout = pipeline_layout,
            .desc_set_layout = desc_set_layout,
            .descriptor_set = descriptor_sets[0],
        };
    }

    pub fn deinit(self: *HZBPassResources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.desc_set_layout, null);
    }
};

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const HZBReduceFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    depth: fg.ResourceUsageHandle,
    hzb_texture: fg.ResourceUsageHandle,
    hzb_properties: gpu_texture_properties.GPUTextureProperties,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    allocator: std.mem.Allocator,
    depth_buffer_usage_handle: fg.ResourceUsageHandle,
    tiled_lighting_frame: *const tiled_lighting_common.TiledLightingFrame,
) !HZBReduceFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("HZB Reduce", false);

    const depth = try builder.readTexture(
        pass_handle,
        depth_buffer_usage_handle,
        .{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_read_bit = true },
            .image_layout = .depth_read_only_optimal,
        },
        &.{},
    );

    // NOTE: HZB size is rounded to match the depth used in the tiled lighting
    // raster pass.
    var hzb_properties = gpu_texture_properties.defaultTextureProperties(
        tiled_lighting_frame.tile_count_x * 8,
        tiled_lighting_frame.tile_count_y * 8,
        hzb_format,
        .{ .storage = true, .sampled = true },
    );
    hzb_properties.mip_count = hzb_mip_count; // FIXME

    // One single-mip view per level, so the reduce shader can write each of
    // them as a storage image.
    const hzb_mip_views = try allocator.alloc(gpu_texture_view.GPUTextureView, hzb_properties.mip_count);

    for (hzb_mip_views, 0..) |*view, i| {
        view.* = gpu_texture_view.defaultTextureView(hzb_properties);
        view.subresource.mip_count = 1;
        view.subresource.mip_offset = @intCast(i);
    }

    const hzb_texture = try builder.createTexture(
        pass_handle,
        "HZB Texture",
        hzb_properties,
        .{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_write_bit = true },
            .image_layout = .general,
        },
        hzb_mip_views,
    );

    return .{
        .pass_handle = pass_handle,
        .depth = depth,
        .hzb_texture = hzb_texture,
        .hzb_properties = hzb_properties,
    };
}

// --------------------------------------------------------------------------
// Descriptor updates
// --------------------------------------------------------------------------

pub fn updateDescriptorSet(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: HZBReduceFrameGraphRecord,
    resources: *const HZBPassResources,
    sampler_resources: SamplerResources,
) void {
    const scene_depth = frame_graph_resources.getTexture(framegraph, record.depth);
    const hzb_texture = frame_graph_resources.getTexture(framegraph, record.hzb_texture);

    const set = resources.descriptor_set;

    write_helper.appendSampler(set, Binding.linear_clamp_sampler, sampler_resources.linear_clamp);
    write_helper.appendImage(set, Binding.scene_depth, .sampled_image, scene_depth.default_view_handle, scene_depth.image_layout);

    std.debug.assert(hzb_texture.additional_views.len == hzb_texture.properties.mip_count);

    write_helper.appendTextureArray(
        set,
        Binding.hzb_mips,
        .storage_image,
        hzb_texture.additional_views,
        hzb_texture.image_layout,
    );
}

// --------------------------------------------------------------------------
// Recording
// --------------------------------------------------------------------------

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: HZBReduceFrameGraphRecord,
    resources: *const HZBPassResources,
    depth_extent: vk.Extent2D,
    hzb_extent: vk.Extent2D,
) void {
    const scope = gpu_scope.begin(vkd, cmd_buffer, @src(), "HZB Reduce");
    defer scope.end(vkd, cmd_buffer);

    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(resources.pipeline_index));

    const push_constants = hlsl_hzb.HZBReducePushConstants{
        .depth_extent_ts_inv = .{
            .x = 1.0 / @as(f32, @floatFromInt(depth_extent.width)),
            .y = 1.0 / @as(f32, @floatFromInt(depth_extent.height)),
        },
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        resources.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(@TypeOf(push_constants)),
        &push_constants,
    );

    const pass_descriptors = [_]vk.DescriptorSet{resources.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.pipeline_layout, 0, &pass_descriptors, &.{});

    vkd.cmdDispatch(
        cmd_buffer,
        compute_helper.divRoundUp(hzb_extent.width, hlsl_hzb.HZBReduceThreadCountX),
        compute_helper.divRoundUp(hzb_extent.height, hlsl_hzb.HZBReduceThreadCountY),
        1,
    );
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "the mip array never exceeds what the shader declares" {
    // The descriptor array is HZBMaxMipCount long and partially bound; writing
    // more views than that would be out of range.
    try testing.expect(hzb_mip_count <= hlsl_hzb.HZBMaxMipCount);
    try testing.expectEqual(hlsl_hzb.HZBMaxMipCount, Binding.bindings[Binding.hzb_mips].count);
}

test "descriptor bindings are dense and correctly ordered" {
    for (Binding.bindings, 0..) |binding, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
    }

    try testing.expectEqual(vk.DescriptorType.sampler, Binding.bindings[Binding.linear_clamp_sampler].type);
    try testing.expectEqual(vk.DescriptorType.sampled_image, Binding.bindings[Binding.scene_depth].type);
    try testing.expectEqual(vk.DescriptorType.storage_image, Binding.bindings[Binding.hzb_mips].type);
}

test "the smallest mip still has at least one texel" {
    // The HZB is tile_count * 8 on each axis, so even a one-tile viewport has
    // 8 texels — enough for 4 mips.
    const smallest: u32 = 8 >> @intCast(hzb_mip_count - 1);
    try testing.expect(smallest >= 1);
}
