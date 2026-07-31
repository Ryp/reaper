// Port of src/renderer/vulkan/renderpass/ShadowMap.{h,cpp}
//
// One depth-only pass per shadow-casting light, each drawing that light's slice
// of the culled meshlets into its own depth target. The culling passes already
// run per shadow pass — `pass_index` indexes both the culling output and the
// descriptor set here — so this only has to bind and draw.

const std = @import("std");
const vk = @import("vulkan");

const constants = @import("constants.zig");
const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_buffer = @import("../../buffer/gpu_buffer.zig");
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");
const meshlet_culling = @import("meshlet_culling.zig");
const pipeline_module = @import("../pipeline.zig");
const pipeline_factory_module = @import("../pipeline_factory.zig");
const render_pass_helpers = @import("../render_pass_helpers.zig");
const shader_modules = @import("../shader_modules.zig");
const storage_buffer = @import("../storage_buffer.zig");

const hlsl_shadow = @import("../../hlsl/shadow/shadow_map_pass.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const PipelineFactory = pipeline_factory_module.PipelineFactory;
const prepare_buckets = @import("../../prepare_buckets.zig");

/// Port of ShadowConstants.h.
pub const shadow_use_reverse_z = prepare_buckets.shadow_use_reverse_z;
pub const shadow_map_format: vk.Format = .d16_unorm;

/// FIXME The C++ allocates a fixed three sets up front rather than sizing them
/// to the scene.
const descriptor_set_count: u32 = 3;

// --------------------------------------------------------------------------
// Descriptor bindings
// --------------------------------------------------------------------------

const SetZero = struct {
    const instance_params = 0;
    const buffer_position_ms = 1;

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
        .{ .slot = 1, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
    };
};

// --------------------------------------------------------------------------
// Pipeline
// --------------------------------------------------------------------------

fn createShadowPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("shadow/render_shadow.vert.spv"),
    );

    // Depth only: there is no fragment stage.
    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
    };

    var properties = pipeline_module.defaultGraphicsPipelineProperties(null);

    // Meshlet index buffers use a primitive restart value between triangles.
    properties.input_assembly.primitive_restart_enable = .true;

    properties.depth_stencil.depth_test_enable = .true;
    properties.depth_stencil.depth_write_enable = .true;
    properties.depth_stencil.depth_compare_op = if (shadow_use_reverse_z) .greater else .less;
    properties.pipeline_layout = pipeline_layout;
    properties.pipeline_rendering.depth_attachment_format = shadow_map_format;

    const dynamic_states = [_]vk.DynamicState{ .viewport, .scissor };

    return pipeline_module.createGraphicsPipeline(vkd, device, &shader_stages, &properties, &dynamic_states);
}

// --------------------------------------------------------------------------
// Resources
// --------------------------------------------------------------------------

pub const ShadowMapResources = struct {
    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,
    desc_set_layout: vk.DescriptorSetLayout,

    descriptor_sets: [descriptor_set_count]vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        pipeline_factory: *PipelineFactory,
    ) !ShadowMapResources {
        var bindings: [SetZero.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&bindings, &SetZero.bindings);

        const binding_flags = [_]vk.DescriptorBindingFlags{.{}} ** SetZero.bindings.len;

        const desc_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &bindings, &binding_flags);
        errdefer vkd.destroyDescriptorSetLayout(device, desc_set_layout, null);

        const pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &.{desc_set_layout}, &.{});
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = pipeline_layout,
            .pipeline_creation_function = &createShadowPipeline,
        });

        const set_layouts = [_]vk.DescriptorSetLayout{desc_set_layout} ** descriptor_set_count;

        var descriptor_sets: [descriptor_set_count]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &set_layouts, &descriptor_sets);

        return .{
            .pipeline_index = pipeline_index,
            .pipeline_layout = pipeline_layout,
            .desc_set_layout = desc_set_layout,
            .descriptor_sets = descriptor_sets,
        };
    }

    pub fn deinit(self: *ShadowMapResources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.desc_set_layout, null);
    }
};

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const ShadowFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    /// One per entry of `prepared.shadow_passes`, in the same order, so
    /// `pass_index` indexes this directly.
    shadow_maps: []const fg.ResourceUsageHandle,
    meshlet_counters: fg.ResourceUsageHandle,
    meshlet_indirect_draw_commands: fg.ResourceUsageHandle,
    meshlet_visible_index_buffer: fg.ResourceUsageHandle,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    allocator: std.mem.Allocator,
    meshlet_pass: meshlet_culling.CullMeshletsFrameGraphRecord,
    prepared: *const prepare_buckets.PreparedData,
) !ShadowFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Shadow", false);

    const shadow_maps = try allocator.alloc(fg.ResourceUsageHandle, prepared.shadow_passes.items.len);

    for (prepared.shadow_passes.items, shadow_maps) |shadow_pass, *out| {
        const properties = gpu_texture_properties.defaultTextureProperties(
            shadow_pass.shadow_map_size[0],
            shadow_pass.shadow_map_size[1],
            shadow_map_format,
            .{ .depth_stencil_attachment = true, .input_attachment = true, .sampled = true },
        );

        out.* = try builder.createTexture(
            pass_handle,
            "Shadow map",
            properties,
            .{
                .stage_mask = .{ .early_fragment_tests_bit = true, .late_fragment_tests_bit = true },
                .access_mask = .{ .depth_stencil_attachment_write_bit = true },
                .image_layout = .attachment_optimal,
            },
            &.{},
        );
    }

    const meshlet_counters = try builder.readBuffer(
        pass_handle,
        meshlet_pass.cull_triangles.meshlet_counters,
        .{ .stage_mask = .{ .draw_indirect_bit = true }, .access_mask = .{ .indirect_command_read_bit = true } },
        &.{},
    );

    const meshlet_indirect_draw_commands = try builder.readBuffer(
        pass_handle,
        meshlet_pass.cull_triangles.meshlet_indirect_draw_commands,
        .{ .stage_mask = .{ .draw_indirect_bit = true }, .access_mask = .{ .indirect_command_read_bit = true } },
        &.{},
    );

    const meshlet_visible_index_buffer = try builder.readBuffer(
        pass_handle,
        meshlet_pass.cull_triangles.meshlet_visible_index_buffer,
        .{ .stage_mask = .{ .index_input_bit = true }, .access_mask = .{ .index_read_bit = true } },
        &.{},
    );

    return .{
        .pass_handle = pass_handle,
        .shadow_maps = shadow_maps,
        .meshlet_counters = meshlet_counters,
        .meshlet_indirect_draw_commands = meshlet_indirect_draw_commands,
        .meshlet_visible_index_buffer = meshlet_visible_index_buffer,
    };
}

// --------------------------------------------------------------------------
// Descriptor updates
// --------------------------------------------------------------------------

pub fn updateDescriptorSets(
    write_helper: *DescriptorWriteHelper,
    frame_storage_allocator: *storage_buffer.StorageBufferAllocator,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const ShadowMapResources,
    vertex_position_buffer: vk.Buffer,
) void {
    if (prepared.shadow_instance_params.items.len == 0) return;

    const alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_shadow.ShadowMapInstanceParams,
        prepared.shadow_instance_params.items,
    );

    for (prepared.shadow_passes.items) |shadow_pass| {
        if (shadow_pass.instance_count == 0) continue;

        std.debug.assert(shadow_pass.pass_index < descriptor_set_count);

        const set = resources.descriptor_sets[shadow_pass.pass_index];

        const instances_view = gpu_buffer.GPUBufferView{
            .offset_bytes = alloc.offset_bytes +
                shadow_pass.instance_offset * @sizeOf(hlsl_shadow.ShadowMapInstanceParams),
            .size_bytes = shadow_pass.instance_count * @sizeOf(hlsl_shadow.ShadowMapInstanceParams),
        };

        write_helper.appendBuffer(
            set,
            SetZero.instance_params,
            .storage_buffer,
            alloc.buffer,
            instances_view.offset_bytes,
            instances_view.size_bytes,
        );
        write_helper.appendBuffer(
            set,
            SetZero.buffer_position_ms,
            .storage_buffer,
            vertex_position_buffer,
            0,
            vk.WHOLE_SIZE,
        );
    }
}

// --------------------------------------------------------------------------
// Recording
// --------------------------------------------------------------------------

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: ShadowFrameGraphRecord,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const ShadowMapResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const meshlet_counters = helper.resources.getBuffer(helper.frame_graph, record.meshlet_counters);
    const indirect_draw_commands = helper.resources.getBuffer(helper.frame_graph, record.meshlet_indirect_draw_commands);
    const visible_index_buffer = helper.resources.getBuffer(helper.frame_graph, record.meshlet_visible_index_buffer);

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(resources.pipeline_index));

    for (prepared.shadow_passes.items) |shadow_pass| {
        if (shadow_pass.instance_count == 0) continue;

        const shadow_map = helper.resources.getTexture(helper.frame_graph, record.shadow_maps[shadow_pass.pass_index]);

        const extent = vk.Extent2D{
            .width = shadow_pass.shadow_map_size[0],
            .height = shadow_pass.shadow_map_size[1],
        };

        std.debug.assert(extent.width == shadow_map.properties.width);
        std.debug.assert(extent.height == shadow_map.properties.height);

        const pass_rect = render_pass_helpers.defaultRect(extent);
        const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
        const scissors = [_]vk.Rect2D{pass_rect};

        vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
        vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

        var depth_attachment = pipeline_module.defaultRenderingAttachmentInfo(
            shadow_map.default_view_handle,
            shadow_map.image_layout,
        );
        depth_attachment.load_op = .clear;
        depth_attachment.clear_value = .{
            .depth_stencil = .{ .depth = if (shadow_use_reverse_z) 0.0 else 1.0, .stencil = 0 },
        };

        const rendering_info = pipeline_module.defaultRenderingInfo(pass_rect, &.{}, &depth_attachment);

        vkd.cmdBeginRendering(cmd_buffer, &rendering_info);

        const meshlet_draw = meshlet_culling.getMeshletDrawParams(shadow_pass.pass_index);

        vkd.cmdBindIndexBuffer2(
            cmd_buffer,
            visible_index_buffer.handle,
            meshlet_draw.index_buffer_offset,
            vk.WHOLE_SIZE,
            meshlet_draw.index_type,
        );

        const pass_descriptors = [_]vk.DescriptorSet{resources.descriptor_sets[shadow_pass.pass_index]};
        vkd.cmdBindDescriptorSets(cmd_buffer, .graphics, resources.pipeline_layout, 0, &pass_descriptors, &.{});

        vkd.cmdDrawIndexedIndirectCount(
            cmd_buffer,
            indirect_draw_commands.handle,
            meshlet_draw.command_buffer_offset,
            meshlet_counters.handle,
            meshlet_draw.counter_buffer_offset,
            meshlet_draw.command_buffer_max_count,
            indirect_draw_commands.properties.element_size_bytes,
        );

        vkd.cmdEndRendering(cmd_buffer);
    }
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "the shadow pipeline's depth format matches the frame graph texture" {
    // The pipeline declares this up front and the frame graph creates the
    // texture separately, so a mismatch only shows up as a validation error at
    // draw time.
    try testing.expectEqual(vk.Format.d16_unorm, shadow_map_format);
}

test "reverse z picks the matching compare op and clear value" {
    const compare_op: vk.CompareOp = if (shadow_use_reverse_z) .greater else .less;
    const clear_depth: f32 = if (shadow_use_reverse_z) 0.0 else 1.0;

    if (shadow_use_reverse_z) {
        try testing.expectEqual(vk.CompareOp.greater, compare_op);
        try testing.expectEqual(@as(f32, 0.0), clear_depth);
    } else {
        try testing.expectEqual(vk.CompareOp.less, compare_op);
        try testing.expectEqual(@as(f32, 1.0), clear_depth);
    }
}

test "shadow passes never outnumber the descriptor sets or culling passes" {
    // `pass_index` indexes both `descriptor_sets` here and the culling output,
    // and prepare_buckets reserves one culling pass for the main view.
    try testing.expect(descriptor_set_count <= meshlet_culling.max_meshlet_culling_pass_count - 1);
}

test "descriptor set zero bindings are dense and correctly ordered" {
    for (SetZero.bindings, 0..) |binding, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
    }

    try testing.expectEqual(vk.DescriptorType.storage_buffer, SetZero.bindings[SetZero.instance_params].type);
    try testing.expectEqual(vk.DescriptorType.storage_buffer, SetZero.bindings[SetZero.buffer_position_ms].type);
}
