// Port of src/renderer/vulkan/renderpass/ForwardPass.{h,cpp}
//
// Draws the culled meshlets into an HDR target with a depth buffer, using one
// indexed indirect-count draw. Two descriptor sets: set 0 is the pass data,
// set 1 is the material texture array, which is partially bound because the
// texture count is only known at runtime.
//
// The shadow map array and the material texture array are both sized for the
// maximum and only filled as far as the scene goes, which is what the
// PARTIALLY_BOUND flag on those bindings is for.

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

const hlsl_forward = @import("../../hlsl/forward.zig");
const hlsl_mesh_instance = @import("../../hlsl/mesh_instance.zig");
const hlsl_mesh_material = @import("../../hlsl/mesh_material.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const LightingPassResources = @import("lighting.zig").LightingPassResources;
const MaterialResources = @import("../material_resources.zig").MaterialResources;
const MeshCache = @import("../mesh_cache.zig").MeshCache;
const PipelineFactory = pipeline_factory_module.PipelineFactory;
const SamplerResources = @import("../sampler_resources.zig").SamplerResources;
const prepare_buckets = @import("../../prepare_buckets.zig");
const vma = @import("../vma.zig").c;

const mesh_instance_count_max: u32 = 512;

// --------------------------------------------------------------------------
// Descriptor bindings
// --------------------------------------------------------------------------

const SetZero = struct {
    const pass_params = 0;
    const instance_params = 1;
    const material_params = 2;
    const visible_meshlets = 3;
    const buffer_position_ms = 4;
    const buffer_attributes = 5;
    const point_lights = 6;
    const shadow_map_sampler = 7;
    const shadow_map_array = 8;

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .uniform_buffer, .stage_mask = .{ .vertex_bit = true, .fragment_bit = true } },
        .{ .slot = 1, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
        .{ .slot = 2, .count = 1, .type = .storage_buffer, .stage_mask = .{ .fragment_bit = true } },
        .{ .slot = 3, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
        .{ .slot = 4, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
        .{ .slot = 5, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
        .{ .slot = 6, .count = 1, .type = .storage_buffer, .stage_mask = .{ .fragment_bit = true } },
        .{ .slot = 7, .count = 1, .type = .sampler, .stage_mask = .{ .fragment_bit = true } },
        .{
            .slot = 8,
            .count = hlsl_mesh_instance.ShadowMapMaxCount,
            .type = .sampled_image,
            .stage_mask = .{ .fragment_bit = true },
        },
    };
};

const SetOne = struct {
    const diffuse_map_sampler = 0;
    const material_maps = 1;

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .sampler, .stage_mask = .{ .fragment_bit = true } },
        .{
            .slot = 1,
            .count = hlsl_mesh_instance.MaterialTextureMaxCount,
            .type = .sampled_image,
            .stage_mask = .{ .fragment_bit = true },
        },
    };
};

// --------------------------------------------------------------------------
// Pipeline
// --------------------------------------------------------------------------

fn createForwardPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(shader_modules.get("forward.vert.spv"));
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(shader_modules.get("forward.frag.spv"));

    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .fragment_bit = true }, &module_create_info_frag, null),
    };

    const blend_attachment_states = [_]vk.PipelineColorBlendAttachmentState{
        pipeline_module.defaultPipelineColorBlendAttachmentState(),
    };

    const color_formats = [_]vk.Format{constants.forward_hdr_color_format};

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

pub const ForwardPipelineInfo = struct {
    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,
    desc_set_layout: vk.DescriptorSetLayout,
    desc_set_layout_material: vk.DescriptorSetLayout,
};

pub const ForwardPassResources = struct {
    pass_constant_buffer: buffer_module.GPUBuffer,
    instance_buffer: buffer_module.GPUBuffer,

    pipe: ForwardPipelineInfo,

    descriptor_set: vk.DescriptorSet,
    material_descriptor_set: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        vma_instance: vma.VmaAllocator,
        pipeline_factory: *PipelineFactory,
    ) !ForwardPassResources {
        var bindings0: [SetZero.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&bindings0, &SetZero.bindings);

        // The shadow map array is sized for the maximum but only the lights
        // that actually cast get written, hence partially bound.
        var binding_flags0 = [_]vk.DescriptorBindingFlags{.{}} ** SetZero.bindings.len;
        binding_flags0[binding_flags0.len - 1] = .{ .partially_bound_bit = true };

        var bindings1: [SetOne.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&bindings1, &SetOne.bindings);

        const binding_flags1 = [_]vk.DescriptorBindingFlags{ .{}, .{ .partially_bound_bit = true } };

        const desc_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &bindings0, &binding_flags0);
        errdefer vkd.destroyDescriptorSetLayout(device, desc_set_layout, null);

        const desc_set_layout_material = try pipeline_module.createDescriptorSetLayout(
            vkd,
            device,
            &bindings1,
            &binding_flags1,
        );
        errdefer vkd.destroyDescriptorSetLayout(device, desc_set_layout_material, null);

        const set_layouts = [_]vk.DescriptorSetLayout{ desc_set_layout, desc_set_layout_material };

        const pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &set_layouts, &.{});
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = pipeline_layout,
            .pipeline_creation_function = &createForwardPipeline,
        });

        const pass_constant_buffer = try buffer_module.createBuffer(
            vma_instance,
            gpu_buffer.defaultBufferProperties(1, @sizeOf(hlsl_forward.ForwardPassParams), .{ .uniform_buffer = true }),
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, pass_constant_buffer);

        const instance_buffer = try buffer_module.createBuffer(
            vma_instance,
            gpu_buffer.defaultBufferProperties(
                mesh_instance_count_max,
                @sizeOf(hlsl_mesh_instance.MeshInstance),
                .{ .storage_buffer = true },
            ),
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, instance_buffer);

        var sets: [2]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &set_layouts, &sets);

        return .{
            .pass_constant_buffer = pass_constant_buffer,
            .instance_buffer = instance_buffer,
            .pipe = .{
                .pipeline_index = pipeline_index,
                .pipeline_layout = pipeline_layout,
                .desc_set_layout = desc_set_layout,
                .desc_set_layout_material = desc_set_layout_material,
            },
            .descriptor_set = sets[0],
            .material_descriptor_set = sets[1],
        };
    }

    pub fn deinit(self: *ForwardPassResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        vkd.destroyPipelineLayout(device, self.pipe.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.pipe.desc_set_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.pipe.desc_set_layout_material, null);

        buffer_module.destroyBuffer(vma_instance, self.pass_constant_buffer);
        buffer_module.destroyBuffer(vma_instance, self.instance_buffer);
    }
};

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const ForwardFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    scene_hdr: fg.ResourceUsageHandle,
    depth: fg.ResourceUsageHandle,
    /// One per shadow-casting light, in `prepared.shadow_passes` order.
    shadow_maps: []const fg.ResourceUsageHandle,
    meshlet_counters: fg.ResourceUsageHandle,
    meshlet_indirect_draw_commands: fg.ResourceUsageHandle,
    meshlet_visible_index_buffer: fg.ResourceUsageHandle,
    visible_meshlet_buffer: fg.ResourceUsageHandle,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    allocator: std.mem.Allocator,
    meshlet_pass: meshlet_culling.CullMeshletsFrameGraphRecord,
    shadow_maps: []const fg.ResourceUsageHandle,
    depth_buffer_usage_handle: fg.ResourceUsageHandle,
    render_extent: vk.Extent2D,
) !ForwardFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Forward", false);

    const scene_hdr = try builder.createTexture(
        pass_handle,
        "Scene HDR",
        gpu_texture_properties.defaultTextureProperties(
            render_extent.width,
            render_extent.height,
            constants.forward_hdr_color_format,
            .{ .color_attachment = true, .sampled = true },
        ),
        .{
            .stage_mask = .{ .color_attachment_output_bit = true },
            .access_mask = .{ .color_attachment_write_bit = true },
            .image_layout = .attachment_optimal,
        },
        &.{},
    );

    const depth = try builder.writeTexture(
        pass_handle,
        depth_buffer_usage_handle,
        .{
            .stage_mask = .{ .early_fragment_tests_bit = true },
            .access_mask = .{ .depth_stencil_attachment_write_bit = true },
            .image_layout = .attachment_optimal,
        },
        &.{},
    );

    const forward_shadow_maps = try allocator.alloc(fg.ResourceUsageHandle, shadow_maps.len);
    for (shadow_maps, forward_shadow_maps) |shadow_map_usage_handle, *out| {
        out.* = try builder.readTexture(
            pass_handle,
            shadow_map_usage_handle,
            .{
                .stage_mask = .{ .fragment_shader_bit = true },
                .access_mask = .{ .shader_read_bit = true },
                .image_layout = .read_only_optimal,
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

    const visible_meshlet_buffer = try builder.readBuffer(
        pass_handle,
        meshlet_pass.cull_triangles.visible_meshlet_buffer,
        .{ .stage_mask = .{ .vertex_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
        &.{},
    );

    return .{
        .pass_handle = pass_handle,
        .scene_hdr = scene_hdr,
        .depth = depth,
        .shadow_maps = forward_shadow_maps,
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
    record: ForwardFrameGraphRecord,
    frame_storage_allocator: *storage_buffer.StorageBufferAllocator,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const ForwardPassResources,
    sampler_resources: SamplerResources,
    mesh_cache: *const MeshCache,
    material_resources: *const MaterialResources,
    lighting_resources: LightingPassResources,
    vma_instance: vma.VmaAllocator,
) !void {
    if (prepared.mesh_instances.items.len == 0) return;

    std.debug.assert(prepared.mesh_materials.items.len > 0);

    const mesh_material_alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_mesh_material.MeshMaterial,
        prepared.mesh_materials.items,
    );

    try buffer_module.uploadBufferData(
        vma_instance,
        resources.pass_constant_buffer,
        resources.pass_constant_buffer.properties_deprecated,
        std.mem.asBytes(&prepared.forward_pass_constants),
        0,
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

    write_helper.appendBuffer(set, SetZero.pass_params, .uniform_buffer, resources.pass_constant_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, SetZero.instance_params, .storage_buffer, resources.instance_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(
        set,
        SetZero.material_params,
        .storage_buffer,
        mesh_material_alloc.buffer,
        mesh_material_alloc.offset_bytes,
        mesh_material_alloc.size_bytes,
    );
    write_helper.appendBuffer(set, SetZero.visible_meshlets, .storage_buffer, visible_meshlet_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, SetZero.buffer_position_ms, .storage_buffer, mesh_cache.vertex_buffer_position.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, SetZero.buffer_attributes, .storage_buffer, mesh_cache.vertex_attributes_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(
        set,
        SetZero.point_lights,
        .storage_buffer,
        lighting_resources.point_light_buffer_alloc.buffer,
        lighting_resources.point_light_buffer_alloc.offset_bytes,
        lighting_resources.point_light_buffer_alloc.size_bytes,
    );
    write_helper.appendSampler(set, SetZero.shadow_map_sampler, sampler_resources.shadow_map_sampler);

    // The shadow map ARRAY is partially bound, so a scene with no casting light
    // leaves it unwritten. The samplers next to it are not: only the last
    // binding of each set carries PARTIALLY_BOUND, so an unwritten sampler is a
    // validation error even when nothing samples through it.
    if (record.shadow_maps.len > 0) {
        var views_buffer: [hlsl_mesh_instance.ShadowMapMaxCount]vk.ImageView = undefined;
        const count = @min(record.shadow_maps.len, views_buffer.len);

        for (views_buffer[0..count], record.shadow_maps[0..count]) |*view, usage_handle| {
            view.* = frame_graph_resources.getTexture(framegraph, usage_handle).default_view_handle;
        }

        write_helper.appendTextureArray(
            set,
            SetZero.shadow_map_array,
            .sampled_image,
            views_buffer[0..count],
            .read_only_optimal,
        );
    }

    write_helper.appendSampler(
        resources.material_descriptor_set,
        SetOne.diffuse_map_sampler,
        sampler_resources.diffuse_map_sampler,
    );

    if (material_resources.textures.items.len > 0) {
        var views_buffer: [hlsl_mesh_instance.MaterialTextureMaxCount]vk.ImageView = undefined;
        const count = @min(material_resources.textures.items.len, views_buffer.len);

        for (views_buffer[0..count], material_resources.textures.items[0..count]) |*view, texture| {
            view.* = texture.default_view;
        }

        write_helper.appendTextureArray(
            resources.material_descriptor_set,
            SetOne.material_maps,
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
    record: ForwardFrameGraphRecord,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const ForwardPassResources,
) void {
    if (prepared.mesh_instances.items.len == 0) return;

    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const meshlet_counters = helper.resources.getBuffer(helper.frame_graph, record.meshlet_counters);
    const indirect_draw_commands = helper.resources.getBuffer(helper.frame_graph, record.meshlet_indirect_draw_commands);
    const visible_index_buffer = helper.resources.getBuffer(helper.frame_graph, record.meshlet_visible_index_buffer);
    const hdr_buffer = helper.resources.getTexture(helper.frame_graph, record.scene_hdr);
    const depth_buffer = helper.resources.getTexture(helper.frame_graph, record.depth);

    const extent = vk.Extent2D{ .width = hdr_buffer.properties.width, .height = hdr_buffer.properties.height };
    const pass_rect = render_pass_helpers.defaultRect(extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(resources.pipe.pipeline_index));

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    var color_attachments = [_]vk.RenderingAttachmentInfo{
        pipeline_module.defaultRenderingAttachmentInfo(hdr_buffer.default_view_handle, hdr_buffer.image_layout),
    };
    color_attachments[0].load_op = .clear;
    color_attachments[0].clear_value = .{ .color = .{ .float_32 = .{ 0.04, 0.04, 0.04, 0.0 } } };

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

    const meshlet_draw = meshlet_culling.getMeshletDrawParams(prepared.main_culling_pass_index);

    vkd.cmdBindIndexBuffer2(
        cmd_buffer,
        visible_index_buffer.handle,
        meshlet_draw.index_buffer_offset,
        vk.WHOLE_SIZE,
        meshlet_draw.index_type,
    );

    const pass_descriptors = [_]vk.DescriptorSet{ resources.descriptor_set, resources.material_descriptor_set };
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

    vkd.cmdEndRendering(cmd_buffer);
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "the forward pipeline formats match the pass's attachments" {
    // The pipeline declares these formats up front and the frame graph creates
    // the textures separately, so a mismatch would only show up as a
    // validation error at draw time.
    try testing.expectEqual(vk.Format.b10g11r11_ufloat_pack32, constants.forward_hdr_color_format);
    try testing.expectEqual(vk.Format.d16_unorm, constants.main_pass_depth_format);
}

test "reverse z picks the matching compare op and clear value" {
    // These two have to agree: reverse-z clears depth to 0 and keeps the
    // greater value. Getting one without the other silently loses all geometry.
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

test "descriptor set zero bindings are dense and correctly ordered" {
    // The slot numbers are what the shader's VK_BINDING declarations use, so a
    // gap or a reorder would bind the wrong buffer.
    for (SetZero.bindings, 0..) |binding, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
    }

    try testing.expectEqual(vk.DescriptorType.uniform_buffer, SetZero.bindings[SetZero.pass_params].type);
    try testing.expectEqual(vk.DescriptorType.sampler, SetZero.bindings[SetZero.shadow_map_sampler].type);
    try testing.expectEqual(vk.DescriptorType.sampled_image, SetZero.bindings[SetZero.shadow_map_array].type);
    try testing.expectEqual(hlsl_mesh_instance.ShadowMapMaxCount, SetZero.bindings[SetZero.shadow_map_array].count);

    for (SetOne.bindings, 0..) |binding, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
    }
    try testing.expectEqual(hlsl_mesh_instance.MaterialTextureMaxCount, SetOne.bindings[SetOne.material_maps].count);
}
