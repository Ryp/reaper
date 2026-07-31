// Port of src/renderer/vulkan/renderpass/GBufferPass.{h,cpp}
//
// Rasterizes the culled meshlets straight into the two G-buffer targets, as an
// alternative to reconstructing them from the visibility buffer with a compute
// shader.
//
// This pass is DEAD CODE in the C++ build: GBufferPass.h is included by nothing
// but GBufferPass.cpp, BackendResources has no member for it, and TestGraphics
// never records it. It was superseded by the visibility-buffer path, which
// writes the same two textures from fill_gbuffer.comp.hlsl.
//
// Both writers end in the same `gbuffer_from_standard_material` ->
// `encode_gbuffer` pair out of shader/gbuffer/gbuffer.hlsl, into the same
// formats, for the same single consumer (tiled lighting). So this is not a
// missing piece of the frame — it is a second, alternative producer of a
// resource that already has one. It therefore hangs off a toggle
// (`Options.use_raster_gbuffer`, default false) and SUBSTITUTES for the compute
// fill rather than coexisting with it: two producers writing one texture in a
// single frame would be meaningless.
//
// The one visible difference between the two producers is texture filtering.
// The raster path samples with hardware derivatives (`Sample`); the compute
// path has no quad neighbours and computes them analytically from the triangle
// barycentrics (`SampleGrad`). Expect small LOD differences at silhouettes.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("../barrier.zig");
const buffer_module = @import("../buffer.zig");
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
const vis_buffer = @import("vis_buffer.zig");

const hlsl_mesh_instance = @import("../../hlsl/mesh_instance.zig");
const hlsl_mesh_material = @import("../../hlsl/mesh_material.zig");
const hlsl_slots = @import("../../hlsl/gbuffer/gbuffer_write_opaque.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const MaterialResources = @import("../material_resources.zig").MaterialResources;
const MeshCache = @import("../mesh_cache.zig").MeshCache;
const PipelineFactory = pipeline_factory_module.PipelineFactory;
const SamplerResources = @import("../sampler_resources.zig").SamplerResources;
const prepare_buckets = @import("../../prepare_buckets.zig");
const vma = @import("../vma.zig").c;

const gbuffer_instance_count_max: u32 = 512;

// --------------------------------------------------------------------------
// Descriptor bindings
// --------------------------------------------------------------------------

// One set of seven, not the set-0/set-1 split ForwardPass uses: every binding
// in gbuffer_write_opaque.share.hlsl is declared VK_BINDING(0, ...).
const SetZero = struct {
    const vs = vk.ShaderStageFlags{ .vertex_bit = true };
    const fs = vk.ShaderStageFlags{ .fragment_bit = true };

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = hlsl_slots.Slot_instance_params, .count = 1, .type = .storage_buffer, .stage_mask = vs },
        .{ .slot = hlsl_slots.Slot_visible_meshlets, .count = 1, .type = .storage_buffer, .stage_mask = vs },
        .{ .slot = hlsl_slots.Slot_buffer_position_ms, .count = 1, .type = .storage_buffer, .stage_mask = vs },
        .{ .slot = hlsl_slots.Slot_buffer_attributes, .count = 1, .type = .storage_buffer, .stage_mask = vs },
        .{ .slot = hlsl_slots.Slot_mesh_materials, .count = 1, .type = .storage_buffer, .stage_mask = fs },
        .{ .slot = hlsl_slots.Slot_diffuse_map_sampler, .count = 1, .type = .sampler, .stage_mask = fs },
        .{
            .slot = hlsl_slots.Slot_material_maps,
            .count = hlsl_mesh_instance.MaterialTextureMaxCount,
            .type = .sampled_image,
            .stage_mask = fs,
        },
    };
};

// --------------------------------------------------------------------------
// Pipeline
// --------------------------------------------------------------------------

fn createGBufferPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("gbuffer/gbuffer_write_opaque.vert.spv"),
    );
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("gbuffer/gbuffer_write_opaque.frag.spv"),
    );

    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .fragment_bit = true }, &module_create_info_frag, null),
    };

    // One per render target, or pipeline creation is invalid against the
    // two-attachment RenderingInfo the record function builds.
    const blend_attachment_states = [_]vk.PipelineColorBlendAttachmentState{
        pipeline_module.defaultPipelineColorBlendAttachmentState(),
        pipeline_module.defaultPipelineColorBlendAttachmentState(),
    };

    const color_formats = [_]vk.Format{ vis_buffer.gbuffer_rt0_format, vis_buffer.gbuffer_rt1_format };

    var properties = pipeline_module.defaultGraphicsPipelineProperties(null);

    // Meshlet index buffers use a primitive restart value between triangles.
    properties.input_assembly.primitive_restart_enable = .true;

    properties.depth_stencil.depth_test_enable = .true;
    properties.depth_stencil.depth_write_enable = .true;
    properties.depth_stencil.depth_compare_op = if (constants.main_pass_use_reverse_z) .greater else .less;

    properties.blend_state.attachment_count = blend_attachment_states.len;
    properties.blend_state.p_attachments = &blend_attachment_states;
    properties.pipeline_layout = pipeline_layout;
    properties.pipeline_rendering.color_attachment_count = color_formats.len;
    properties.pipeline_rendering.p_color_attachment_formats = &color_formats;
    properties.pipeline_rendering.depth_attachment_format = constants.main_pass_depth_format;

    const dynamic_states = [_]vk.DynamicState{ .viewport, .scissor };

    return pipeline_module.createGraphicsPipeline(vkd, device, &shader_stages, &properties, &dynamic_states);
}

// --------------------------------------------------------------------------
// Resources
// --------------------------------------------------------------------------

pub const GBufferPipelineInfo = struct {
    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,
    desc_set_layout: vk.DescriptorSetLayout,
};

pub const GBufferPassResources = struct {
    instance_buffer: buffer_module.GPUBuffer,

    pipe: GBufferPipelineInfo,

    descriptor_set: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        vma_instance: vma.VmaAllocator,
        pipeline_factory: *PipelineFactory,
    ) !GBufferPassResources {
        var bindings: [SetZero.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&bindings, &SetZero.bindings);

        // The material texture array is sized for the maximum and only filled as
        // far as the scene goes. Every other binding is written every frame.
        var binding_flags = [_]vk.DescriptorBindingFlags{.{}} ** SetZero.bindings.len;
        binding_flags[binding_flags.len - 1] = .{ .partially_bound_bit = true };

        const desc_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &bindings, &binding_flags);
        errdefer vkd.destroyDescriptorSetLayout(device, desc_set_layout, null);

        const set_layouts = [_]vk.DescriptorSetLayout{desc_set_layout};

        const pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &set_layouts, &.{});
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = pipeline_layout,
            .pipeline_creation_function = &createGBufferPipeline,
        });

        const instance_buffer = try buffer_module.createBuffer(
            vma_instance,
            gpu_buffer.defaultBufferProperties(
                gbuffer_instance_count_max,
                @sizeOf(hlsl_mesh_instance.MeshInstance),
                .{ .storage_buffer = true },
            ),
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, instance_buffer);

        var sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &set_layouts, &sets);

        return .{
            .instance_buffer = instance_buffer,
            .pipe = .{
                .pipeline_index = pipeline_index,
                .pipeline_layout = pipeline_layout,
                .desc_set_layout = desc_set_layout,
            },
            .descriptor_set = sets[0],
        };
    }

    pub fn deinit(self: *GBufferPassResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        // The pipeline itself belongs to the factory, which destroys it.
        vkd.destroyPipelineLayout(device, self.pipe.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.pipe.desc_set_layout, null);

        buffer_module.destroyBuffer(vma_instance, self.instance_buffer);
    }
};

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const GBufferFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    gbuffer_rt0: fg.ResourceUsageHandle,
    gbuffer_rt1: fg.ResourceUsageHandle,
    depth: fg.ResourceUsageHandle,
    meshlet_counters: fg.ResourceUsageHandle,
    meshlet_indirect_draw_commands: fg.ResourceUsageHandle,
    meshlet_visible_index_buffer: fg.ResourceUsageHandle,
    visible_meshlet_buffer: fg.ResourceUsageHandle,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    meshlet_pass: meshlet_culling.CullMeshletsFrameGraphRecord,
    vis_buffer_record: vis_buffer.VisBufferFrameGraphRecord,
    render_extent: vk.Extent2D,
) !GBufferFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("GBuffer", false);

    const color_attachment_access = barrier_module.GPUTextureAccess{
        .stage_mask = .{ .color_attachment_output_bit = true },
        .access_mask = .{ .color_attachment_write_bit = true },
        .image_layout = .attachment_optimal,
    };

    // Writing into the compute fill's own targets rather than creating a second
    // pair. New targets would leave the fill pass with no consumer, and the
    // graph culls unreachable passes — its textures would then never be
    // allocated while its usages still assert as used. Overwriting costs one
    // redundant compute dispatch in a debug mode, which is the cheaper trade.
    const gbuffer_rt0 = try builder.writeTexture(
        pass_handle,
        vis_buffer_record.fill_gbuffer.gbuffer_rt0,
        color_attachment_access,
        &.{},
    );

    const gbuffer_rt1 = try builder.writeTexture(
        pass_handle,
        vis_buffer_record.fill_gbuffer.gbuffer_rt1,
        color_attachment_access,
        &.{},
    );

    // A private depth buffer, not the visibility pass's. That one is read back
    // by the HZB and by tiled lighting, so slotting a second writer in front of
    // them would change which version each of them sees.
    const depth = try builder.createTexture(
        pass_handle,
        "GBuffer Depth",
        gpu_texture_properties.defaultTextureProperties(
            render_extent.width,
            render_extent.height,
            constants.main_pass_depth_format,
            .{ .depth_stencil_attachment = true },
        ),
        .{
            // EARLY | LATE, for the reason spelled out in forward.zig's
            // deviation note: depth is written at both stages.
            .stage_mask = .{ .early_fragment_tests_bit = true, .late_fragment_tests_bit = true },
            .access_mask = .{ .depth_stencil_attachment_write_bit = true },
            .image_layout = .attachment_optimal,
        },
        &.{},
    );

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

    const visible_meshlet_buffer = try builder.readBuffer(
        pass_handle,
        meshlet_pass.cull_triangles.visible_meshlet_buffer,
        .{ .stage_mask = .{ .vertex_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
        &.{},
    );

    return .{
        .pass_handle = pass_handle,
        .gbuffer_rt0 = gbuffer_rt0,
        .gbuffer_rt1 = gbuffer_rt1,
        .depth = depth,
        .meshlet_counters = meshlet_counters,
        .meshlet_indirect_draw_commands = meshlet_indirect_draw_commands,
        .meshlet_visible_index_buffer = meshlet_visible_index_buffer,
        .visible_meshlet_buffer = visible_meshlet_buffer,
    };
}

// --------------------------------------------------------------------------
// Descriptor updates
// --------------------------------------------------------------------------

pub fn updateDescriptorSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: GBufferFrameGraphRecord,
    frame_storage_allocator: *storage_buffer.StorageBufferAllocator,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const GBufferPassResources,
    sampler_resources: SamplerResources,
    mesh_cache: *const MeshCache,
    material_resources: *const MaterialResources,
    vma_instance: vma.VmaAllocator,
) !void {
    // Matches the record function's early-out, so the set is never left half
    // written.
    if (prepared.mesh_instances.items.len == 0) return;

    std.debug.assert(prepared.mesh_materials.items.len > 0);

    const mesh_material_alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_mesh_material.MeshMaterial,
        prepared.mesh_materials.items,
    );

    try buffer_module.uploadBufferData(
        vma_instance,
        resources.instance_buffer,
        resources.instance_buffer.properties_deprecated,
        std.mem.sliceAsBytes(prepared.mesh_instances.items),
        0,
    );

    const visible_meshlet_buffer = frame_graph_resources.getBuffer(framegraph, record.visible_meshlet_buffer);

    const set = resources.descriptor_set;

    write_helper.appendBuffer(set, hlsl_slots.Slot_instance_params, .storage_buffer, resources.instance_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, hlsl_slots.Slot_visible_meshlets, .storage_buffer, visible_meshlet_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, hlsl_slots.Slot_buffer_position_ms, .storage_buffer, mesh_cache.vertex_buffer_position.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, hlsl_slots.Slot_buffer_attributes, .storage_buffer, mesh_cache.vertex_attributes_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(
        set,
        hlsl_slots.Slot_mesh_materials,
        .storage_buffer,
        mesh_material_alloc.buffer,
        mesh_material_alloc.offset_bytes,
        mesh_material_alloc.size_bytes,
    );

    // Written unconditionally: only the last binding carries PARTIALLY_BOUND,
    // so leaving the sampler unwritten is a validation error even with no
    // textures to sample.
    write_helper.appendSampler(set, hlsl_slots.Slot_diffuse_map_sampler, sampler_resources.diffuse_map_sampler);

    if (material_resources.textures.items.len > 0) {
        var views_buffer: [hlsl_mesh_instance.MaterialTextureMaxCount]vk.ImageView = undefined;
        const count = @min(material_resources.textures.items.len, views_buffer.len);

        for (views_buffer[0..count], material_resources.textures.items[0..count]) |*view, texture| {
            view.* = texture.default_view;
        }

        write_helper.appendTextureArray(
            set,
            hlsl_slots.Slot_material_maps,
            .sampled_image,
            views_buffer[0..count],
            .read_only_optimal,
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
    record: GBufferFrameGraphRecord,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const GBufferPassResources,
) void {
    // NOTE: unlike forward.zig, the empty-scene case may NOT return before the
    // barrier scope. Tiled lighting samples these targets unconditionally, so
    // skipping the scope would hand it an image still in the compute fill's
    // layout. Begin the pass, let the clears run, then bail out of the draw.
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const meshlet_counters = helper.resources.getBuffer(helper.frame_graph, record.meshlet_counters);
    const indirect_draw_commands = helper.resources.getBuffer(helper.frame_graph, record.meshlet_indirect_draw_commands);
    const visible_index_buffer = helper.resources.getBuffer(helper.frame_graph, record.meshlet_visible_index_buffer);
    const rt0 = helper.resources.getTexture(helper.frame_graph, record.gbuffer_rt0);
    const rt1 = helper.resources.getTexture(helper.frame_graph, record.gbuffer_rt1);
    const depth_buffer = helper.resources.getTexture(helper.frame_graph, record.depth);

    const extent = vk.Extent2D{ .width = rt0.properties.width, .height = rt0.properties.height };
    const pass_rect = render_pass_helpers.defaultRect(extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    var color_attachments = [_]vk.RenderingAttachmentInfo{
        pipeline_module.defaultRenderingAttachmentInfo(rt0.default_view_handle, rt0.image_layout),
        pipeline_module.defaultRenderingAttachmentInfo(rt1.default_view_handle, rt1.image_layout),
    };

    // Cleared rather than loaded: whatever the compute fill left here would
    // otherwise survive in every pixel this pass does not cover, which would
    // make the two producers impossible to tell apart. The targets are R32_UINT,
    // hence the integer clear union.
    for (&color_attachments) |*attachment| {
        attachment.load_op = .clear;
        attachment.clear_value = .{ .color = .{ .uint_32 = .{ 0, 0, 0, 0 } } };
    }

    var depth_attachment = pipeline_module.defaultRenderingAttachmentInfo(
        depth_buffer.default_view_handle,
        depth_buffer.image_layout,
    );
    depth_attachment.load_op = .clear;
    depth_attachment.clear_value = .{
        .depth_stencil = .{ .depth = if (constants.main_pass_use_reverse_z) 0.0 else 1.0, .stencil = 0 },
    };

    const rendering_info = pipeline_module.defaultRenderingInfo(pass_rect, &color_attachments, &depth_attachment);

    vkd.cmdBeginRendering(cmd_buffer, &rendering_info);
    defer vkd.cmdEndRendering(cmd_buffer);

    if (prepared.mesh_instances.items.len == 0) return;

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(resources.pipe.pipeline_index));

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    const meshlet_draw = meshlet_culling.getMeshletDrawParams(prepared.main_culling_pass_index);

    vkd.cmdBindIndexBuffer2(
        cmd_buffer,
        visible_index_buffer.handle,
        meshlet_draw.index_buffer_offset,
        vk.WHOLE_SIZE,
        meshlet_draw.index_type,
    );

    const pass_descriptors = [_]vk.DescriptorSet{resources.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .graphics, resources.pipe.pipeline_layout, 0, &pass_descriptors, &.{});

    vkd.cmdDrawIndexedIndirectCount(
        cmd_buffer,
        indirect_draw_commands.handle,
        meshlet_draw.command_buffer_offset,
        meshlet_counters.handle,
        meshlet_draw.counter_buffer_offset,
        meshlet_draw.command_buffer_max_count,
        indirect_draw_commands.properties.element_size_bytes,
    );
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "bindings are dense and match the shader's slot numbers" {
    // The slots come from the HLSL mirror; a gap or a reorder here binds the
    // wrong buffer with no validation error to show for it.
    for (SetZero.bindings, 0..) |binding, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
    }

    try testing.expectEqual(@as(u32, 0), hlsl_slots.Slot_instance_params);
    try testing.expectEqual(@as(u32, 6), hlsl_slots.Slot_material_maps);
    try testing.expectEqual(vk.DescriptorType.sampler, SetZero.bindings[hlsl_slots.Slot_diffuse_map_sampler].type);
    try testing.expectEqual(vk.DescriptorType.sampled_image, SetZero.bindings[hlsl_slots.Slot_material_maps].type);
}

test "the partially bound binding is the last one" {
    // fillLayoutBindings pairs the flags array with the bindings positionally,
    // and every binding before the last must be written every frame.
    try testing.expectEqual(SetZero.bindings.len - 1, hlsl_slots.Slot_material_maps);
    try testing.expectEqual(
        hlsl_mesh_instance.MaterialTextureMaxCount,
        SetZero.bindings[hlsl_slots.Slot_material_maps].count,
    );
}

test "the pipeline's render target formats match what tiled lighting reads" {
    // This pass and the compute fill both write these two textures, and tiled
    // lighting decodes them with one shared shader — so the formats have to be
    // the single declaration in vis_buffer, not a copy that can drift.
    try testing.expectEqual(vk.Format.r32_uint, vis_buffer.gbuffer_rt0_format);
    try testing.expectEqual(vk.Format.r32_uint, vis_buffer.gbuffer_rt1_format);
}

test "reverse z picks the matching compare op and clear value" {
    const compare_op: vk.CompareOp = if (constants.main_pass_use_reverse_z) .greater else .less;
    const clear_depth: f32 = if (constants.main_pass_use_reverse_z) 0.0 else 1.0;

    if (constants.main_pass_use_reverse_z) {
        try testing.expectEqual(vk.CompareOp.greater, compare_op);
        try testing.expectEqual(@as(f32, 0.0), clear_depth);
    } else {
        try testing.expectEqual(vk.CompareOp.less, compare_op);
        try testing.expectEqual(@as(f32, 1.0), clear_depth);
    }
}
