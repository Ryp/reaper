// Port of src/renderer/vulkan/renderpass/VisibilityBufferPass.{h,cpp}
//
// Two passes. The raster pass draws the culled meshlets into a single R32_UINT
// target that encodes (meshlet, triangle) per pixel plus the depth buffer. The
// fill pass is a compute shader that reads that back, refetches the vertex data
// for the hit triangle, and writes the packed G-buffer.
//
// The MSAA variants are carried faithfully: the vis buffer and depth are
// rendered at half resolution with 4x sample locations, and depth is resolved
// either by the fill shader (when the device supports compute stores to depth)
// or by a separate fullscreen pass. `backend.options.enable_msaa_visibility`
// defaults to false and only ImGui flips it, so the non-MSAA path is what runs.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("../barrier.zig");
const compute_helper = @import("../compute_helper.zig");
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

const hlsl_fill_gbuffer = @import("../../hlsl/vis_buffer/fill_gbuffer.zig");
const hlsl_mesh_instance = @import("../../hlsl/mesh_instance.zig");
const hlsl_mesh_material = @import("../../hlsl/mesh_material.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const MaterialResources = @import("../material_resources.zig").MaterialResources;
const MeshCache = @import("../mesh_cache.zig").MeshCache;
const PipelineFactory = pipeline_factory_module.PipelineFactory;
const SamplerResources = @import("../sampler_resources.zig").SamplerResources;
const prepare_buckets = @import("../../prepare_buckets.zig");

/// Port of VisibilityBufferConstants.h and GBufferPassConstants.h.
pub const visibility_buffer_format: vk.Format = .r32_uint;
pub const gbuffer_rt0_format: vk.Format = .r32_uint;
pub const gbuffer_rt1_format: vk.Format = .r32_uint;

const msaa_samples: u32 = 4;

/// NOTE: origin is in the top-left corner.
const msaa_4x_grid_sample_locations = [msaa_samples]vk.SampleLocationEXT{
    .{ .x = 0.25, .y = 0.25 },
    .{ .x = 0.75, .y = 0.25 },
    .{ .x = 0.25, .y = 0.75 },
    .{ .x = 0.75, .y = 0.75 },
};

// --------------------------------------------------------------------------
// Descriptor bindings
// --------------------------------------------------------------------------

const Render = struct {
    const instance_params = 0;
    const visible_meshlets = 1;
    const buffer_position_ms = 2;

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
        .{ .slot = 1, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
        .{ .slot = 2, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
    };
};

const FillGBuffer = struct {
    const slots = hlsl_fill_gbuffer;

    const compute_only = vk.ShaderStageFlags{ .compute_bit = true };

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = slots.Slot_VisBuffer, .count = 1, .type = .sampled_image, .stage_mask = compute_only },
        // MSAA-only
        .{ .slot = slots.Slot_VisBufferDepthMS, .count = 1, .type = .sampled_image, .stage_mask = compute_only },
        // MSAA-only
        .{ .slot = slots.Slot_ResolvedDepth, .count = 1, .type = .storage_image, .stage_mask = compute_only },
        .{ .slot = slots.Slot_GBuffer0, .count = 1, .type = .storage_image, .stage_mask = compute_only },
        .{ .slot = slots.Slot_GBuffer1, .count = 1, .type = .storage_image, .stage_mask = compute_only },
        .{ .slot = slots.Slot_instance_params, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{ .slot = slots.Slot_visible_index_buffer, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{ .slot = slots.Slot_buffer_position_ms, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{ .slot = slots.Slot_buffer_attributes, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{ .slot = slots.Slot_visible_meshlets, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{ .slot = slots.Slot_mesh_materials, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{ .slot = slots.Slot_diffuse_map_sampler, .count = 1, .type = .sampler, .stage_mask = compute_only },
        .{
            .slot = slots.Slot_material_maps,
            .count = hlsl_mesh_instance.MaterialTextureMaxCount,
            .type = .sampled_image,
            .stage_mask = compute_only,
        },
    };
};

// --------------------------------------------------------------------------
// Pipelines
// --------------------------------------------------------------------------

fn createVisBufferPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
    comptime enable_msaa: bool,
) !vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("vis_buffer/vis_buffer_raster.vert.spv"),
    );
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("vis_buffer/vis_buffer_raster.frag.spv"),
    );

    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .fragment_bit = true }, &module_create_info_frag, null),
    };

    const blend_attachment_states = [_]vk.PipelineColorBlendAttachmentState{
        pipeline_module.defaultPipelineColorBlendAttachmentState(),
    };

    const color_formats = [_]vk.Format{visibility_buffer_format};

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

    const sample_location_info = vk.PipelineSampleLocationsStateCreateInfoEXT{
        .s_type = .pipeline_sample_locations_state_create_info_ext,
        .p_next = null,
        .sample_locations_enable = .true,
        .sample_locations_info = .{
            .s_type = .sample_locations_info_ext,
            .p_next = null,
            .sample_locations_per_pixel = sampleCountToVulkan(msaa_samples),
            .sample_location_grid_size = .{ .width = 1, .height = 1 },
            .sample_locations_count = msaa_4x_grid_sample_locations.len,
            .p_sample_locations = &msaa_4x_grid_sample_locations,
        },
    };

    if (enable_msaa) {
        properties.multisample.rasterization_samples = sampleCountToVulkan(msaa_samples);
        properties.multisample.p_next = &sample_location_info;
    }

    const dynamic_states = [_]vk.DynamicState{ .viewport, .scissor };

    return pipeline_module.createGraphicsPipeline(vkd, device, &shader_stages, &properties, &dynamic_states);
}

fn createVisBufferPipelineNonMsaa(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    return createVisBufferPipeline(vkd, device, pipeline_layout, false);
}

fn createVisBufferPipelineMsaa(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    return createVisBufferPipeline(vkd, device, pipeline_layout, true);
}

fn computePipelineCreator(comptime shader_name: []const u8) pipeline_factory_module.PipelineFunctor {
    return struct {
        fn create(
            vkd: *const vk.DeviceWrapper,
            device: vk.Device,
            pipeline_layout: vk.PipelineLayout,
        ) anyerror!vk.Pipeline {
            const module_create_info = pipeline_module.shaderModuleCreateInfo(shader_modules.get(shader_name));

            const shader_stage = pipeline_module.defaultPipelineShaderStageCreateInfo(
                .{ .compute_bit = true },
                &module_create_info,
                null,
            );

            return pipeline_module.createComputePipeline(vkd, device, pipeline_layout, shader_stage);
        }
    }.create;
}

fn createLegacyDepthResolvePipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("fullscreen_triangle.vert.spv"),
    );
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("vis_buffer/resolve_depth_legacy.frag.spv"),
    );

    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .fragment_bit = true }, &module_create_info_frag, null),
    };

    var properties = pipeline_module.defaultGraphicsPipelineProperties(null);
    properties.depth_stencil.depth_test_enable = .true;
    properties.depth_stencil.depth_write_enable = .true;
    properties.depth_stencil.depth_compare_op = .always;
    properties.pipeline_layout = pipeline_layout;
    properties.pipeline_rendering.depth_attachment_format = constants.main_pass_depth_format;

    const dynamic_states = [_]vk.DynamicState{ .viewport, .scissor };

    return pipeline_module.createGraphicsPipeline(vkd, device, &shader_stages, &properties, &dynamic_states);
}

fn sampleCountToVulkan(sample_count: u32) vk.SampleCountFlags {
    return switch (sample_count) {
        1 => .{ .@"1_bit" = true },
        2 => .{ .@"2_bit" = true },
        4 => .{ .@"4_bit" = true },
        8 => .{ .@"8_bit" = true },
        16 => .{ .@"16_bit" = true },
        32 => .{ .@"32_bit" = true },
        64 => .{ .@"64_bit" = true },
        else => unreachable,
    };
}

// --------------------------------------------------------------------------
// Resources
// --------------------------------------------------------------------------

pub const PipelineInfo = struct {
    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,
    desc_set_layout: vk.DescriptorSetLayout,

    fn deinit(self: PipelineInfo, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.desc_set_layout, null);
    }
};

pub const VisibilityBufferPassResources = struct {
    pipe: PipelineInfo,
    pipe_msaa: PipelineInfo,
    descriptor_set: vk.DescriptorSet,

    fill_pipe: PipelineInfo,
    fill_pipe_msaa: PipelineInfo,
    fill_pipe_msaa_with_resolve: PipelineInfo,
    descriptor_set_fill: vk.DescriptorSet,

    legacy_resolve_pipe: PipelineInfo,
    descriptor_set_legacy_resolve: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        pipeline_factory: *PipelineFactory,
    ) !VisibilityBufferPassResources {
        // The two raster pipelines take the same layout but are registered
        // separately, matching the C++.
        const pipe = try createRenderPipelineInfo(vkd, device, pipeline_factory, &createVisBufferPipelineNonMsaa);
        errdefer pipe.deinit(vkd, device);

        const pipe_msaa = try createRenderPipelineInfo(vkd, device, pipeline_factory, &createVisBufferPipelineMsaa);
        errdefer pipe_msaa.deinit(vkd, device);

        const fill_pipe = try createFillPipelineInfo(
            vkd,
            device,
            pipeline_factory,
            computePipelineCreator("vis_buffer/fill_gbuffer.comp.spv"),
        );
        errdefer fill_pipe.deinit(vkd, device);

        const fill_pipe_msaa = try createFillPipelineInfo(
            vkd,
            device,
            pipeline_factory,
            computePipelineCreator("vis_buffer/fill_gbuffer_msaa.comp.spv"),
        );
        errdefer fill_pipe_msaa.deinit(vkd, device);

        const fill_pipe_msaa_with_resolve = try createFillPipelineInfo(
            vkd,
            device,
            pipeline_factory,
            computePipelineCreator("vis_buffer/fill_gbuffer_msaa_with_depth_resolve.comp.spv"),
        );
        errdefer fill_pipe_msaa_with_resolve.deinit(vkd, device);

        const legacy_resolve_pipe = blk: {
            const layout_bindings = [_]vk.DescriptorSetLayoutBinding{.{
                .binding = 0,
                .descriptor_type = .sampled_image,
                .descriptor_count = 1,
                .stage_flags = .{ .fragment_bit = true },
                .p_immutable_samplers = null,
            }};

            const desc_set_layout = try pipeline_module.createDescriptorSetLayout(
                vkd,
                device,
                &layout_bindings,
                &[_]vk.DescriptorBindingFlags{.{}},
            );
            errdefer vkd.destroyDescriptorSetLayout(device, desc_set_layout, null);

            const pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &.{desc_set_layout}, &.{});
            errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

            break :blk PipelineInfo{
                .pipeline_index = try pipeline_factory.registerPipelineCreator(.{
                    .pipeline_layout = pipeline_layout,
                    .pipeline_creation_function = &createLegacyDepthResolvePipeline,
                }),
                .pipeline_layout = pipeline_layout,
                .desc_set_layout = desc_set_layout,
            };
        };
        errdefer legacy_resolve_pipe.deinit(vkd, device);

        var render_sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(
            vkd,
            device,
            descriptor_pool,
            &.{pipe.desc_set_layout},
            &render_sets,
        );

        var descriptor_set_fill: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(
            vkd,
            device,
            descriptor_pool,
            &.{fill_pipe.desc_set_layout},
            &descriptor_set_fill,
        );

        var descriptor_set_legacy_resolve: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(
            vkd,
            device,
            descriptor_pool,
            &.{legacy_resolve_pipe.desc_set_layout},
            &descriptor_set_legacy_resolve,
        );

        return .{
            .pipe = pipe,
            .pipe_msaa = pipe_msaa,
            .descriptor_set = render_sets[0],
            .fill_pipe = fill_pipe,
            .fill_pipe_msaa = fill_pipe_msaa,
            .fill_pipe_msaa_with_resolve = fill_pipe_msaa_with_resolve,
            .descriptor_set_fill = descriptor_set_fill[0],
            .legacy_resolve_pipe = legacy_resolve_pipe,
            .descriptor_set_legacy_resolve = descriptor_set_legacy_resolve[0],
        };
    }

    pub fn deinit(self: *VisibilityBufferPassResources, vkd: anytype, device: vk.Device) void {
        self.pipe.deinit(vkd, device);
        self.pipe_msaa.deinit(vkd, device);
        self.fill_pipe.deinit(vkd, device);
        self.fill_pipe_msaa.deinit(vkd, device);
        self.fill_pipe_msaa_with_resolve.deinit(vkd, device);
        self.legacy_resolve_pipe.deinit(vkd, device);
    }
};

fn createRenderPipelineInfo(
    vkd: anytype,
    device: vk.Device,
    pipeline_factory: *PipelineFactory,
    creation_function: pipeline_factory_module.PipelineFunctor,
) !PipelineInfo {
    var layout_bindings: [Render.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
    descriptor_set.fillLayoutBindings(&layout_bindings, &Render.bindings);

    const binding_flags = [_]vk.DescriptorBindingFlags{.{}} ** Render.bindings.len;

    const desc_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &layout_bindings, &binding_flags);
    errdefer vkd.destroyDescriptorSetLayout(device, desc_set_layout, null);

    const pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &.{desc_set_layout}, &.{});
    errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

    return .{
        .pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = pipeline_layout,
            .pipeline_creation_function = creation_function,
        }),
        .pipeline_layout = pipeline_layout,
        .desc_set_layout = desc_set_layout,
    };
}

fn createFillPipelineInfo(
    vkd: anytype,
    device: vk.Device,
    pipeline_factory: *PipelineFactory,
    creation_function: pipeline_factory_module.PipelineFunctor,
) !PipelineInfo {
    var layout_bindings: [FillGBuffer.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
    descriptor_set.fillLayoutBindings(&layout_bindings, &FillGBuffer.bindings);

    // The material texture array is sized for the maximum and only filled as
    // far as the scene goes.
    var binding_flags = [_]vk.DescriptorBindingFlags{.{}} ** FillGBuffer.bindings.len;
    binding_flags[binding_flags.len - 1] = .{ .partially_bound_bit = true };

    const desc_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &layout_bindings, &binding_flags);
    errdefer vkd.destroyDescriptorSetLayout(device, desc_set_layout, null);

    const push_constant_ranges = [_]vk.PushConstantRange{.{
        .stage_flags = .{ .compute_bit = true },
        .offset = 0,
        .size = @sizeOf(hlsl_fill_gbuffer.FillGBufferPushConstants),
    }};

    const pipeline_layout = try pipeline_module.createPipelineLayout(
        vkd,
        device,
        &.{desc_set_layout},
        &push_constant_ranges,
    );
    errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

    return .{
        .pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = pipeline_layout,
            .pipeline_creation_function = creation_function,
        }),
        .pipeline_layout = pipeline_layout,
        .desc_set_layout = desc_set_layout,
    };
}

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const VisBufferFrameGraphRecord = struct {
    pub const RenderRecord = struct {
        pass_handle: fg.RenderPassHandle,
        vis_buffer: fg.ResourceUsageHandle,
        depth: fg.ResourceUsageHandle,
        meshlet_counters: fg.ResourceUsageHandle,
        meshlet_indirect_draw_commands: fg.ResourceUsageHandle,
        meshlet_visible_index_buffer: fg.ResourceUsageHandle,
        visible_meshlet_buffer: fg.ResourceUsageHandle,
    };

    pub const FillGBufferRecord = struct {
        pass_handle: fg.RenderPassHandle,
        vis_buffer: fg.ResourceUsageHandle,
        vis_buffer_depth_msaa: fg.ResourceUsageHandle,
        resolved_depth: fg.ResourceUsageHandle,
        gbuffer_rt0: fg.ResourceUsageHandle,
        gbuffer_rt1: fg.ResourceUsageHandle,
        meshlet_visible_index_buffer: fg.ResourceUsageHandle,
        visible_meshlet_buffer: fg.ResourceUsageHandle,
    };

    /// Used when MSAA is on and there's no support for shader stores to depth.
    pub const LegacyDepthResolveRecord = struct {
        pass_handle: fg.RenderPassHandle,
        vis_buffer_depth_msaa: fg.ResourceUsageHandle,
        resolved_depth: fg.ResourceUsageHandle,
    };

    render: RenderRecord,
    fill_gbuffer: FillGBufferRecord,
    legacy_depth_resolve: LegacyDepthResolveRecord,

    /// NOTE: dynamically set to whoever produces the usable depth for further
    /// render passes.
    depth: fg.ResourceUsageHandle,

    scene_depth_properties: gpu_texture_properties.GPUTextureProperties,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    meshlet_pass: meshlet_culling.CullMeshletsFrameGraphRecord,
    render_extent: vk.Extent2D,
    enable_msaa: bool,
    support_shader_stores_to_depth: bool,
) !VisBufferFrameGraphRecord {
    var record: VisBufferFrameGraphRecord = undefined;

    var scene_depth_properties = gpu_texture_properties.defaultTextureProperties(
        render_extent.width,
        render_extent.height,
        constants.main_pass_depth_format,
        .{ .depth_stencil_attachment = true, .sampled = true },
    );

    record.depth = fg.ResourceUsageHandle.invalid;
    record.legacy_depth_resolve = .{
        .pass_handle = fg.RenderPassHandle.invalid,
        .vis_buffer_depth_msaa = fg.ResourceUsageHandle.invalid,
        .resolved_depth = fg.ResourceUsageHandle.invalid,
    };

    {
        const visibility = &record.render;

        visibility.pass_handle = try builder.createRenderPass("Visibility", false);

        var vis_buffer_properties = gpu_texture_properties.defaultTextureProperties(
            render_extent.width,
            render_extent.height,
            visibility_buffer_format,
            .{ .color_attachment = true, .sampled = true },
        );

        const color_attachment_access = barrier_module.GPUTextureAccess{
            .stage_mask = .{ .color_attachment_output_bit = true },
            .access_mask = .{ .color_attachment_write_bit = true },
            .image_layout = .attachment_optimal,
        };

        const depth_attachment_access = barrier_module.GPUTextureAccess{
            .stage_mask = .{ .early_fragment_tests_bit = true, .late_fragment_tests_bit = true },
            .access_mask = .{ .depth_stencil_attachment_write_bit = true },
            .image_layout = .attachment_optimal,
        };

        if (enable_msaa) {
            // FIXME dragons be hiding when using even resolutions!
            vis_buffer_properties.width /= 2;
            vis_buffer_properties.height /= 2;
            vis_buffer_properties.sample_count = msaa_samples;

            visibility.vis_buffer = try builder.createTexture(
                visibility.pass_handle,
                "Visibility Buffer MSAA",
                vis_buffer_properties,
                color_attachment_access,
                &.{},
            );

            var vis_depth_msaa_properties = scene_depth_properties;
            vis_depth_msaa_properties.width /= 2;
            vis_depth_msaa_properties.height /= 2;
            vis_depth_msaa_properties.sample_count = msaa_samples;
            vis_depth_msaa_properties.misc_flags.sample_location_compatible = true;

            visibility.depth = try builder.createTexture(
                visibility.pass_handle,
                "Visibility Depth MSAA",
                vis_depth_msaa_properties,
                depth_attachment_access,
                &.{},
            );
        } else {
            visibility.vis_buffer = try builder.createTexture(
                visibility.pass_handle,
                "Visibility Buffer",
                vis_buffer_properties,
                color_attachment_access,
                &.{},
            );

            visibility.depth = try builder.createTexture(
                visibility.pass_handle,
                "Main Depth",
                scene_depth_properties,
                depth_attachment_access,
                &.{},
            );

            record.depth = visibility.depth; // FIXME
        }

        visibility.meshlet_counters = try builder.readBuffer(
            visibility.pass_handle,
            meshlet_pass.cull_triangles.meshlet_counters,
            .{ .stage_mask = .{ .draw_indirect_bit = true }, .access_mask = .{ .indirect_command_read_bit = true } },
            &.{},
        );

        visibility.meshlet_indirect_draw_commands = try builder.readBuffer(
            visibility.pass_handle,
            meshlet_pass.cull_triangles.meshlet_indirect_draw_commands,
            .{ .stage_mask = .{ .draw_indirect_bit = true }, .access_mask = .{ .indirect_command_read_bit = true } },
            &.{},
        );

        visibility.meshlet_visible_index_buffer = try builder.readBuffer(
            visibility.pass_handle,
            meshlet_pass.cull_triangles.meshlet_visible_index_buffer,
            .{ .stage_mask = .{ .index_input_bit = true }, .access_mask = .{ .index_read_bit = true } },
            &.{},
        );

        visibility.visible_meshlet_buffer = try builder.readBuffer(
            visibility.pass_handle,
            meshlet_pass.cull_triangles.visible_meshlet_buffer,
            .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
            &.{},
        );
    }

    {
        const visibility_gbuffer = &record.fill_gbuffer;

        visibility_gbuffer.pass_handle = try builder.createRenderPass("Visibility Fill GBuffer", false);

        const compute_read = barrier_module.GPUTextureAccess{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_read_bit = true },
            .image_layout = .read_only_optimal,
        };

        const compute_write = barrier_module.GPUTextureAccess{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_write_bit = true },
            .image_layout = .general,
        };

        visibility_gbuffer.vis_buffer = try builder.readTexture(
            visibility_gbuffer.pass_handle,
            record.render.vis_buffer,
            compute_read,
            &.{},
        );

        if (enable_msaa and support_shader_stores_to_depth) {
            visibility_gbuffer.vis_buffer_depth_msaa = try builder.readTexture(
                visibility_gbuffer.pass_handle,
                record.render.depth,
                compute_read,
                &.{},
            );

            scene_depth_properties.usage_flags.storage = true;

            visibility_gbuffer.resolved_depth = try builder.createTexture(
                visibility_gbuffer.pass_handle,
                "Main Depth",
                scene_depth_properties,
                compute_write,
                &.{},
            );

            record.depth = visibility_gbuffer.resolved_depth; // FIXME
        } else {
            visibility_gbuffer.vis_buffer_depth_msaa = fg.ResourceUsageHandle.invalid;
            visibility_gbuffer.resolved_depth = fg.ResourceUsageHandle.invalid;
        }

        visibility_gbuffer.gbuffer_rt0 = try builder.createTexture(
            visibility_gbuffer.pass_handle,
            "GBuffer RT0",
            gpu_texture_properties.defaultTextureProperties(
                render_extent.width,
                render_extent.height,
                gbuffer_rt0_format,
                .{ .sampled = true, .storage = true },
            ),
            compute_write,
            &.{},
        );

        visibility_gbuffer.gbuffer_rt1 = try builder.createTexture(
            visibility_gbuffer.pass_handle,
            "GBuffer RT1",
            gpu_texture_properties.defaultTextureProperties(
                render_extent.width,
                render_extent.height,
                gbuffer_rt1_format,
                .{ .sampled = true, .storage = true },
            ),
            compute_write,
            &.{},
        );

        visibility_gbuffer.meshlet_visible_index_buffer = try builder.readBuffer(
            visibility_gbuffer.pass_handle,
            meshlet_pass.cull_triangles.meshlet_visible_index_buffer,
            .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
            &.{},
        );

        visibility_gbuffer.visible_meshlet_buffer = try builder.readBuffer(
            visibility_gbuffer.pass_handle,
            meshlet_pass.cull_triangles.visible_meshlet_buffer,
            .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
            &.{},
        );
    }

    if (enable_msaa and !support_shader_stores_to_depth) {
        const legacy_depth_resolve = &record.legacy_depth_resolve;

        legacy_depth_resolve.pass_handle = try builder.createRenderPass("Legacy Depth Resolve", false);

        legacy_depth_resolve.vis_buffer_depth_msaa = try builder.readTexture(
            legacy_depth_resolve.pass_handle,
            record.render.depth,
            .{
                .stage_mask = .{ .fragment_shader_bit = true },
                .access_mask = .{ .shader_read_bit = true },
                .image_layout = .read_only_optimal,
            },
            &.{},
        );

        legacy_depth_resolve.resolved_depth = try builder.createTexture(
            legacy_depth_resolve.pass_handle,
            "Main Depth",
            scene_depth_properties,
            .{
                .stage_mask = .{ .early_fragment_tests_bit = true, .late_fragment_tests_bit = true },
                .access_mask = .{ .depth_stencil_attachment_write_bit = true },
                .image_layout = .attachment_optimal,
            },
            &.{},
        );

        record.depth = legacy_depth_resolve.resolved_depth; // FIXME
    }

    record.scene_depth_properties = scene_depth_properties;

    return record;
}

// --------------------------------------------------------------------------
// Descriptor updates
// --------------------------------------------------------------------------

pub fn updateDescriptorSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: VisBufferFrameGraphRecord,
    frame_storage_allocator: *storage_buffer.StorageBufferAllocator,
    resources: *const VisibilityBufferPassResources,
    prepared: *const prepare_buckets.PreparedData,
    sampler_resources: SamplerResources,
    material_resources: *const MaterialResources,
    mesh_cache: *const MeshCache,
    enable_msaa: bool,
    support_shader_stores_to_depth: bool,
) void {
    if (prepared.mesh_instances.items.len == 0) return;

    const mesh_instance_alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_mesh_instance.MeshInstance,
        prepared.mesh_instances.items,
    );

    const mesh_material_alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_mesh_material.MeshMaterial,
        prepared.mesh_materials.items,
    );

    {
        const visible_meshlet_buffer = frame_graph_resources.getBuffer(framegraph, record.render.visible_meshlet_buffer);
        const set = resources.descriptor_set;

        write_helper.appendBuffer(
            set,
            Render.instance_params,
            .storage_buffer,
            mesh_instance_alloc.buffer,
            mesh_instance_alloc.offset_bytes,
            mesh_instance_alloc.size_bytes,
        );
        write_helper.appendBuffer(set, Render.visible_meshlets, .storage_buffer, visible_meshlet_buffer.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(set, Render.buffer_position_ms, .storage_buffer, mesh_cache.vertex_buffer_position.handle, 0, vk.WHOLE_SIZE);
    }

    {
        const slots = hlsl_fill_gbuffer;
        const set = resources.descriptor_set_fill;

        const meshlet_visible_index_buffer = frame_graph_resources.getBuffer(
            framegraph,
            record.fill_gbuffer.meshlet_visible_index_buffer,
        );
        const visible_meshlet_buffer = frame_graph_resources.getBuffer(
            framegraph,
            record.fill_gbuffer.visible_meshlet_buffer,
        );
        const vis_buffer = frame_graph_resources.getTexture(framegraph, record.fill_gbuffer.vis_buffer);
        const gbuffer_rt0 = frame_graph_resources.getTexture(framegraph, record.fill_gbuffer.gbuffer_rt0);
        const gbuffer_rt1 = frame_graph_resources.getTexture(framegraph, record.fill_gbuffer.gbuffer_rt1);

        const visible_index_buffer_view = gpu_buffer.getBufferView(
            meshlet_visible_index_buffer.properties,
            meshlet_culling.getMeshletVisibleIndexBufferPass(prepared.main_culling_pass_index),
        );

        write_helper.appendImage(set, slots.Slot_VisBuffer, .sampled_image, vis_buffer.default_view_handle, vis_buffer.image_layout);

        if (enable_msaa and support_shader_stores_to_depth) {
            const vis_buffer_depth_msaa = frame_graph_resources.getTexture(
                framegraph,
                record.fill_gbuffer.vis_buffer_depth_msaa,
            );
            const resolved_depth = frame_graph_resources.getTexture(framegraph, record.fill_gbuffer.resolved_depth);

            write_helper.appendImage(set, slots.Slot_VisBufferDepthMS, .sampled_image, vis_buffer_depth_msaa.default_view_handle, vis_buffer_depth_msaa.image_layout);
            write_helper.appendImage(set, slots.Slot_ResolvedDepth, .storage_image, resolved_depth.default_view_handle, resolved_depth.image_layout);
        }

        write_helper.appendImage(set, slots.Slot_GBuffer0, .storage_image, gbuffer_rt0.default_view_handle, gbuffer_rt0.image_layout);
        write_helper.appendImage(set, slots.Slot_GBuffer1, .storage_image, gbuffer_rt1.default_view_handle, gbuffer_rt1.image_layout);
        write_helper.appendBuffer(
            set,
            slots.Slot_instance_params,
            .storage_buffer,
            mesh_instance_alloc.buffer,
            mesh_instance_alloc.offset_bytes,
            mesh_instance_alloc.size_bytes,
        );
        write_helper.appendBuffer(
            set,
            slots.Slot_visible_index_buffer,
            .storage_buffer,
            meshlet_visible_index_buffer.handle,
            visible_index_buffer_view.offset_bytes,
            visible_index_buffer_view.size_bytes,
        );
        write_helper.appendBuffer(set, slots.Slot_buffer_position_ms, .storage_buffer, mesh_cache.vertex_buffer_position.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(set, slots.Slot_buffer_attributes, .storage_buffer, mesh_cache.vertex_attributes_buffer.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(set, slots.Slot_visible_meshlets, .storage_buffer, visible_meshlet_buffer.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(
            set,
            slots.Slot_mesh_materials,
            .storage_buffer,
            mesh_material_alloc.buffer,
            mesh_material_alloc.offset_bytes,
            mesh_material_alloc.size_bytes,
        );
        write_helper.appendSampler(set, slots.Slot_diffuse_map_sampler, sampler_resources.diffuse_map_sampler);

        if (material_resources.textures.items.len > 0) {
            var views_buffer: [hlsl_mesh_instance.MaterialTextureMaxCount]vk.ImageView = undefined;
            const count = @min(material_resources.textures.items.len, views_buffer.len);

            for (views_buffer[0..count], material_resources.textures.items[0..count]) |*view, texture| {
                view.* = texture.default_view;
            }

            write_helper.appendTextureArray(set, slots.Slot_material_maps, .sampled_image, views_buffer[0..count], .read_only_optimal);
        }
    }

    if (enable_msaa and !support_shader_stores_to_depth) {
        const vis_buffer_depth_msaa = frame_graph_resources.getTexture(
            framegraph,
            record.legacy_depth_resolve.vis_buffer_depth_msaa,
        );

        write_helper.appendImage(
            resources.descriptor_set_legacy_resolve,
            0,
            .sampled_image,
            vis_buffer_depth_msaa.default_view_handle,
            vis_buffer_depth_msaa.image_layout,
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
    record: VisBufferFrameGraphRecord.RenderRecord,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const VisibilityBufferPassResources,
    enable_msaa: bool,
) void {
    // FIXME should be moved out
    if (prepared.mesh_instances.items.len == 0) return;

    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const pipe = if (enable_msaa) resources.pipe_msaa else resources.pipe;

    const meshlet_counters = helper.resources.getBuffer(helper.frame_graph, record.meshlet_counters);
    const indirect_draw_commands = helper.resources.getBuffer(helper.frame_graph, record.meshlet_indirect_draw_commands);
    const visible_index_buffer = helper.resources.getBuffer(helper.frame_graph, record.meshlet_visible_index_buffer);
    const vis_buffer = helper.resources.getTexture(helper.frame_graph, record.vis_buffer);
    const depth_buffer = helper.resources.getTexture(helper.frame_graph, record.depth);

    const extent = vk.Extent2D{ .width = depth_buffer.properties.width, .height = depth_buffer.properties.height };
    const pass_rect = render_pass_helpers.defaultRect(extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(pipe.pipeline_index));

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    var color_attachments = [_]vk.RenderingAttachmentInfo{
        pipeline_module.defaultRenderingAttachmentInfo(vis_buffer.default_view_handle, vis_buffer.image_layout),
    };
    color_attachments[0].load_op = .clear;
    color_attachments[0].clear_value = .{ .color = .{ .uint_32 = .{ 0, 0, 0, 0 } } };

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

    const pass_descriptors = [_]vk.DescriptorSet{resources.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .graphics, pipe.pipeline_layout, 0, &pass_descriptors, &.{});

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

pub fn recordFillGBufferCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: VisBufferFrameGraphRecord.FillGBufferRecord,
    resources: *const VisibilityBufferPassResources,
    render_extent: vk.Extent2D,
    enable_msaa: bool,
    support_shader_stores_to_depth: bool,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const pipe = if (enable_msaa)
        (if (support_shader_stores_to_depth) resources.fill_pipe_msaa_with_resolve else resources.fill_pipe_msaa)
    else
        resources.fill_pipe;

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(pipe.pipeline_index));

    const push_constants = hlsl_fill_gbuffer.FillGBufferPushConstants{
        .extent_ts = .{ render_extent.width, render_extent.height },
        .extent_ts_inv = .{
            1.0 / @as(f32, @floatFromInt(render_extent.width)),
            1.0 / @as(f32, @floatFromInt(render_extent.height)),
        },
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        pipe.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(@TypeOf(push_constants)),
        &push_constants,
    );

    const pass_descriptors = [_]vk.DescriptorSet{resources.descriptor_set_fill};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, pipe.pipeline_layout, 0, &pass_descriptors, &.{});

    vkd.cmdDispatch(
        cmd_buffer,
        compute_helper.divRoundUp(render_extent.width, hlsl_fill_gbuffer.GBufferFillThreadCountX),
        compute_helper.divRoundUp(render_extent.height, hlsl_fill_gbuffer.GBufferFillThreadCountY),
        1,
    );
}

pub fn recordLegacyDepthResolveCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: VisBufferFrameGraphRecord.LegacyDepthResolveRecord,
    resources: *const VisibilityBufferPassResources,
    enable_msaa: bool,
    support_shader_stores_to_depth: bool,
) void {
    if (!(enable_msaa and !support_shader_stores_to_depth)) return;

    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const depth_dst = helper.resources.getTexture(helper.frame_graph, record.resolved_depth);

    const depth_extent = vk.Extent2D{ .width = depth_dst.properties.width, .height = depth_dst.properties.height };
    const pass_rect = render_pass_helpers.defaultRect(depth_extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(resources.legacy_resolve_pipe.pipeline_index));

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    const pass_descriptors = [_]vk.DescriptorSet{resources.descriptor_set_legacy_resolve};
    vkd.cmdBindDescriptorSets(
        cmd_buffer,
        .graphics,
        resources.legacy_resolve_pipe.pipeline_layout,
        0,
        &pass_descriptors,
        &.{},
    );

    var depth_attachment = pipeline_module.defaultRenderingAttachmentInfo(
        depth_dst.default_view_handle,
        depth_dst.image_layout,
    );
    depth_attachment.load_op = .dont_care;

    const rendering_info = pipeline_module.defaultRenderingInfo(pass_rect, &.{}, &depth_attachment);

    vkd.cmdBeginRendering(cmd_buffer, &rendering_info);

    vkd.cmdDraw(cmd_buffer, 3, 1, 0, 0);

    vkd.cmdEndRendering(cmd_buffer);
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "fill-gbuffer bindings match the shader's slot numbers" {
    // These slots come from the shared HLSL header, so a mismatch here means
    // the compute shader reads a different resource than the one bound.
    const slots = hlsl_fill_gbuffer;

    try testing.expectEqual(@as(u32, slots.Slot_VisBuffer), FillGBuffer.bindings[0].slot);
    try testing.expectEqual(@as(u32, slots.Slot_material_maps), FillGBuffer.bindings[FillGBuffer.bindings.len - 1].slot);

    for (FillGBuffer.bindings, 0..) |binding, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
    }

    for (Render.bindings, 0..) |binding, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
    }
}

test "the vis buffer and gbuffer formats are all single-channel uint" {
    // The vis buffer packs (meshlet, triangle) into one uint and the gbuffer
    // packs its channels by hand, so all three are R32_UINT.
    try testing.expectEqual(vk.Format.r32_uint, visibility_buffer_format);
    try testing.expectEqual(vk.Format.r32_uint, gbuffer_rt0_format);
    try testing.expectEqual(vk.Format.r32_uint, gbuffer_rt1_format);
}

test "the msaa sample grid is a regular 4x grid inside the pixel" {
    try testing.expectEqual(@as(usize, msaa_samples), msaa_4x_grid_sample_locations.len);

    for (msaa_4x_grid_sample_locations) |location| {
        try testing.expect(location.x > 0.0 and location.x < 1.0);
        try testing.expect(location.y > 0.0 and location.y < 1.0);
    }

    try testing.expectEqual(vk.SampleCountFlags{ .@"4_bit" = true }, sampleCountToVulkan(msaa_samples));
}
