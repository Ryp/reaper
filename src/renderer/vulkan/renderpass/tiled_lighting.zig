// Port of src/renderer/vulkan/renderpass/TiledLightingPass.{h,cpp}
//
// The deferred shading pass: one compute dispatch reads the packed G-buffer,
// the depth, and its tile's light list, and writes the HDR lighting result. A
// second debug pass turns the per-tile light counts into a false-colour image
// that the swapchain pass can composite.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("../barrier.zig");
const buffer_module = @import("../buffer.zig");
const compute_helper = @import("../compute_helper.zig");
const constants = @import("constants.zig");
const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_buffer = @import("../../buffer/gpu_buffer.zig");
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");
const pipeline_module = @import("../pipeline.zig");
const pipeline_factory_module = @import("../pipeline_factory.zig");
const shader_modules = @import("../shader_modules.zig");
const shadow_map = @import("shadow_map.zig");
const tiled_raster = @import("tiled_raster.zig");
const vis_buffer = @import("vis_buffer.zig");

const hlsl_mesh_instance = @import("../../hlsl/mesh_instance.zig");
const hlsl_tiled_lighting = @import("../../hlsl/tiled_lighting/tiled_lighting.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const LightingPassResources = @import("lighting.zig").LightingPassResources;
const PipelineFactory = pipeline_factory_module.PipelineFactory;
const SamplerResources = @import("../sampler_resources.zig").SamplerResources;
const prepare_buckets = @import("../../prepare_buckets.zig");
const vma = @import("../vma.zig").c;

const lighting_format: vk.Format = .b10g11r11_ufloat_pack32;
const tile_debug_output_format: vk.Format = .r8g8b8a8_unorm;

// --------------------------------------------------------------------------
// Descriptor bindings
// --------------------------------------------------------------------------

const Lighting = struct {
    const constants_buffer = 0;
    const light_list = 1;
    const gbuffer_rt0 = 2;
    const gbuffer_rt1 = 3;
    const main_view_depth = 4;
    const point_lights = 5;
    const shadow_map_sampler = 6;
    const lighting_output = 7;
    const tile_debug = 8;
    const shadow_map_array = 9;

    const compute_only = vk.ShaderStageFlags{ .compute_bit = true };

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .uniform_buffer, .stage_mask = compute_only },
        .{ .slot = 1, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{ .slot = 2, .count = 1, .type = .sampled_image, .stage_mask = compute_only },
        .{ .slot = 3, .count = 1, .type = .sampled_image, .stage_mask = compute_only },
        .{ .slot = 4, .count = 1, .type = .sampled_image, .stage_mask = compute_only },
        .{ .slot = 5, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{ .slot = 6, .count = 1, .type = .sampler, .stage_mask = compute_only },
        .{ .slot = 7, .count = 1, .type = .storage_image, .stage_mask = compute_only },
        .{ .slot = 8, .count = 1, .type = .storage_buffer, .stage_mask = compute_only },
        .{
            .slot = 9,
            .count = hlsl_mesh_instance.ShadowMapMaxCount,
            .type = .sampled_image,
            .stage_mask = compute_only,
        },
    };
};

const Debug = struct {
    const tile_debug = 0;
    const output = 1;

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .storage_buffer, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 1, .count = 1, .type = .storage_image, .stage_mask = .{ .compute_bit = true } },
    };
};

// --------------------------------------------------------------------------
// Pipelines
// --------------------------------------------------------------------------

fn createLightingPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("tiled_lighting/tiled_lighting.comp.spv"),
    );

    const shader_stage = pipeline_module.defaultPipelineShaderStageCreateInfo(
        .{ .compute_bit = true },
        &module_create_info,
        null,
    );

    return pipeline_module.createComputePipeline(vkd, device, pipeline_layout, shader_stage);
}

fn createLightingDebugPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("tiled_lighting/tiled_lighting_debug.comp.spv"),
    );

    const shader_stage = pipeline_module.defaultPipelineShaderStageCreateInfo(
        .{ .compute_bit = true },
        &module_create_info,
        null,
    );

    return pipeline_module.createComputePipeline(vkd, device, pipeline_layout, shader_stage);
}

// --------------------------------------------------------------------------
// Resources
// --------------------------------------------------------------------------

pub const TiledLightingPassResources = struct {
    pub const PipelineInfo = struct {
        descriptor_set_layout: vk.DescriptorSetLayout,
        pipeline_layout: vk.PipelineLayout,
        pipeline_index: u32,
    };

    lighting: PipelineInfo,
    tiled_lighting_descriptor_set: vk.DescriptorSet,
    tiled_lighting_constant_buffer: buffer_module.GPUBuffer,

    debug: PipelineInfo,
    debug_descriptor_set: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        vma_instance: vma.VmaAllocator,
        pipeline_factory: *PipelineFactory,
    ) !TiledLightingPassResources {
        // ---- Lighting ----
        var lighting_layout_bindings: [Lighting.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&lighting_layout_bindings, &Lighting.bindings);

        // Only the shadow map array is partially bound; it happens to be the
        // last binding, but the C++ indexes it by slot, so do the same.
        var lighting_binding_flags = [_]vk.DescriptorBindingFlags{.{}} ** Lighting.bindings.len;
        lighting_binding_flags[Lighting.shadow_map_array] = .{ .partially_bound_bit = true };

        const lighting_set_layout = try pipeline_module.createDescriptorSetLayout(
            vkd,
            device,
            &lighting_layout_bindings,
            &lighting_binding_flags,
        );
        errdefer vkd.destroyDescriptorSetLayout(device, lighting_set_layout, null);

        const lighting_pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{lighting_set_layout},
            &[_]vk.PushConstantRange{.{
                .stage_flags = .{ .compute_bit = true },
                .offset = 0,
                .size = @sizeOf(hlsl_tiled_lighting.TiledLightingPushConstants),
            }},
        );
        errdefer vkd.destroyPipelineLayout(device, lighting_pipeline_layout, null);

        const lighting_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = lighting_pipeline_layout,
            .pipeline_creation_function = &createLightingPipeline,
        });

        var lighting_sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(
            vkd,
            device,
            descriptor_pool,
            &.{lighting_set_layout},
            &lighting_sets,
        );

        const tiled_lighting_constant_buffer = try buffer_module.createBuffer(
            vma_instance,
            gpu_buffer.defaultBufferProperties(
                1,
                @sizeOf(hlsl_tiled_lighting.TiledLightingConstants),
                .{ .uniform_buffer = true },
            ),
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, tiled_lighting_constant_buffer);

        // ---- Debug ----
        var debug_layout_bindings: [Debug.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&debug_layout_bindings, &Debug.bindings);

        const debug_set_layout = try pipeline_module.createDescriptorSetLayout(
            vkd,
            device,
            &debug_layout_bindings,
            &([_]vk.DescriptorBindingFlags{.{}} ** Debug.bindings.len),
        );
        errdefer vkd.destroyDescriptorSetLayout(device, debug_set_layout, null);

        const debug_pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{debug_set_layout},
            &[_]vk.PushConstantRange{.{
                .stage_flags = .{ .compute_bit = true },
                .offset = 0,
                .size = @sizeOf(hlsl_tiled_lighting.TiledLightingDebugPushConstants),
            }},
        );
        errdefer vkd.destroyPipelineLayout(device, debug_pipeline_layout, null);

        const debug_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = debug_pipeline_layout,
            .pipeline_creation_function = &createLightingDebugPipeline,
        });

        var debug_sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &.{debug_set_layout}, &debug_sets);

        return .{
            .lighting = .{
                .descriptor_set_layout = lighting_set_layout,
                .pipeline_layout = lighting_pipeline_layout,
                .pipeline_index = lighting_pipeline_index,
            },
            .tiled_lighting_descriptor_set = lighting_sets[0],
            .tiled_lighting_constant_buffer = tiled_lighting_constant_buffer,
            .debug = .{
                .descriptor_set_layout = debug_set_layout,
                .pipeline_layout = debug_pipeline_layout,
                .pipeline_index = debug_pipeline_index,
            },
            .debug_descriptor_set = debug_sets[0],
        };
    }

    pub fn deinit(
        self: *TiledLightingPassResources,
        vkd: anytype,
        device: vk.Device,
        vma_instance: vma.VmaAllocator,
    ) void {
        vkd.destroyPipelineLayout(device, self.debug.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.debug.descriptor_set_layout, null);

        buffer_module.destroyBuffer(vma_instance, self.tiled_lighting_constant_buffer);

        vkd.destroyPipelineLayout(device, self.lighting.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.lighting.descriptor_set_layout, null);
    }
};

// --------------------------------------------------------------------------
// Frame graph records
// --------------------------------------------------------------------------

pub const TiledLightingFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    shadow_maps: []const fg.ResourceUsageHandle,
    light_list: fg.ResourceUsageHandle,
    depth: fg.ResourceUsageHandle,
    gbuffer_rt0: fg.ResourceUsageHandle,
    gbuffer_rt1: fg.ResourceUsageHandle,
    lighting: fg.ResourceUsageHandle,
    tile_debug_texture: fg.ResourceUsageHandle,
};

pub const TiledLightingDebugFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    tile_debug: fg.ResourceUsageHandle,
    output: fg.ResourceUsageHandle,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    allocator: std.mem.Allocator,
    vis_buffer_record: vis_buffer.VisBufferFrameGraphRecord,
    /// Which producer's G-buffer to decode. Normally the visibility buffer's
    /// compute fill, but the optional raster G-buffer pass writes the same two
    /// textures and substitutes for it — hence a parameter rather than reaching
    /// into vis_buffer_record directly.
    gbuffer_rt0_source: fg.ResourceUsageHandle,
    gbuffer_rt1_source: fg.ResourceUsageHandle,
    shadow: shadow_map.ShadowFrameGraphRecord,
    light_raster_record: tiled_raster.LightRasterFrameGraphRecord,
) !TiledLightingFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Tiled Lighting", false);

    const compute_read = barrier_module.GPUTextureAccess{
        .stage_mask = .{ .compute_shader_bit = true },
        .access_mask = .{ .shader_read_bit = true },
        .image_layout = .read_only_optimal,
    };

    const shadow_maps = try allocator.alloc(fg.ResourceUsageHandle, shadow.shadow_maps.len);

    for (shadow.shadow_maps, shadow_maps) |shadow_map_usage_handle, *out| {
        out.* = try builder.readTexture(pass_handle, shadow_map_usage_handle, compute_read, &.{});
    }

    const light_list = try builder.readBuffer(
        pass_handle,
        light_raster_record.light_raster.light_list,
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
        &.{},
    );

    const gbuffer_rt0 = try builder.readTexture(pass_handle, gbuffer_rt0_source, compute_read, &.{});
    const gbuffer_rt1 = try builder.readTexture(pass_handle, gbuffer_rt1_source, compute_read, &.{});

    const depth = try builder.readTexture(
        pass_handle,
        vis_buffer_record.depth,
        .{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_read_bit = true },
            .image_layout = .depth_read_only_optimal,
        },
        &.{},
    );

    const lighting = try builder.createTexture(
        pass_handle,
        "Lighting",
        gpu_texture_properties.defaultTextureProperties(
            vis_buffer_record.scene_depth_properties.width,
            vis_buffer_record.scene_depth_properties.height,
            lighting_format,
            .{ .storage = true, .sampled = true, .color_attachment = true },
        ),
        .{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_write_bit = true },
            .image_layout = .general,
        },
        &.{},
    );

    const tile_debug_properties = gpu_buffer.defaultBufferProperties(
        light_raster_record.tile_depth_properties.width * light_raster_record.tile_depth_properties.height,
        @sizeOf(hlsl_tiled_lighting.TileDebug),
        .{ .storage_buffer = true },
    );

    const tile_debug_texture = try builder.createBuffer(
        pass_handle,
        "Tile debug",
        tile_debug_properties,
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_write_bit = true } },
        &.{},
    );

    return .{
        .pass_handle = pass_handle,
        .shadow_maps = shadow_maps,
        .light_list = light_list,
        .depth = depth,
        .gbuffer_rt0 = gbuffer_rt0,
        .gbuffer_rt1 = gbuffer_rt1,
        .lighting = lighting,
        .tile_debug_texture = tile_debug_texture,
    };
}

pub fn createDebugFrameGraphRecord(
    builder: *Builder,
    tiled_lighting_record: TiledLightingFrameGraphRecord,
    render_extent: vk.Extent2D,
) !TiledLightingDebugFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Tiled Lighting Debug", false);

    const tile_debug = try builder.readBuffer(
        pass_handle,
        tiled_lighting_record.tile_debug_texture,
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
        &.{},
    );

    const output = try builder.createTexture(
        pass_handle,
        "Tiled Lighting Debug Texture",
        gpu_texture_properties.defaultTextureProperties(
            render_extent.width,
            render_extent.height,
            tile_debug_output_format,
            .{ .storage = true, .sampled = true },
        ),
        .{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_write_bit = true },
            .image_layout = .general,
        },
        &.{},
    );

    return .{ .pass_handle = pass_handle, .tile_debug = tile_debug, .output = output };
}

// --------------------------------------------------------------------------
// Descriptor updates
// --------------------------------------------------------------------------

pub fn updateDescriptorSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: TiledLightingFrameGraphRecord,
    prepared: *const prepare_buckets.PreparedData,
    lighting_resources: LightingPassResources,
    resources: *const TiledLightingPassResources,
    sampler_resources: SamplerResources,
    vma_instance: vma.VmaAllocator,
) !void {
    if (prepared.point_lights.items.len == 0) return;

    try buffer_module.uploadBufferData(
        vma_instance,
        resources.tiled_lighting_constant_buffer,
        resources.tiled_lighting_constant_buffer.properties_deprecated,
        std.mem.asBytes(&prepared.tiled_light_constants),
        0,
    );

    const light_list_buffer = frame_graph_resources.getBuffer(framegraph, record.light_list);
    const gbuffer_rt0 = frame_graph_resources.getTexture(framegraph, record.gbuffer_rt0);
    const gbuffer_rt1 = frame_graph_resources.getTexture(framegraph, record.gbuffer_rt1);
    const main_view_depth = frame_graph_resources.getTexture(framegraph, record.depth);
    const lighting_output = frame_graph_resources.getTexture(framegraph, record.lighting);
    const tile_debug_buffer = frame_graph_resources.getBuffer(framegraph, record.tile_debug_texture);

    const set = resources.tiled_lighting_descriptor_set;

    write_helper.appendBuffer(set, Lighting.constants_buffer, .uniform_buffer, resources.tiled_lighting_constant_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, Lighting.light_list, .storage_buffer, light_list_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendImage(set, Lighting.gbuffer_rt0, .sampled_image, gbuffer_rt0.default_view_handle, gbuffer_rt0.image_layout);
    write_helper.appendImage(set, Lighting.gbuffer_rt1, .sampled_image, gbuffer_rt1.default_view_handle, gbuffer_rt1.image_layout);
    write_helper.appendImage(set, Lighting.main_view_depth, .sampled_image, main_view_depth.default_view_handle, main_view_depth.image_layout);
    write_helper.appendBuffer(
        set,
        Lighting.point_lights,
        .storage_buffer,
        lighting_resources.point_light_buffer_alloc.buffer,
        lighting_resources.point_light_buffer_alloc.offset_bytes,
        lighting_resources.point_light_buffer_alloc.size_bytes,
    );
    write_helper.appendSampler(set, Lighting.shadow_map_sampler, sampler_resources.shadow_map_sampler);
    write_helper.appendImage(set, Lighting.lighting_output, .storage_image, lighting_output.default_view_handle, lighting_output.image_layout);
    write_helper.appendBuffer(set, Lighting.tile_debug, .storage_buffer, tile_debug_buffer.handle, 0, vk.WHOLE_SIZE);

    if (record.shadow_maps.len > 0) {
        var views_buffer: [hlsl_mesh_instance.ShadowMapMaxCount]vk.ImageView = undefined;
        const count = @min(record.shadow_maps.len, views_buffer.len);

        // All shadow maps sit in the same layout, so one is enough to name it.
        var layout: vk.ImageLayout = .read_only_optimal;

        for (views_buffer[0..count], record.shadow_maps[0..count]) |*view, usage_handle| {
            const texture = frame_graph_resources.getTexture(framegraph, usage_handle);
            view.* = texture.default_view_handle;
            layout = texture.image_layout;
        }

        write_helper.appendTextureArray(set, Lighting.shadow_map_array, .sampled_image, views_buffer[0..count], layout);
    }
}

pub fn updateDebugDescriptorSet(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: TiledLightingDebugFrameGraphRecord,
    resources: *const TiledLightingPassResources,
) void {
    const tile_debug_buffer = frame_graph_resources.getBuffer(framegraph, record.tile_debug);
    const tile_debug_texture = frame_graph_resources.getTexture(framegraph, record.output);

    const set = resources.debug_descriptor_set;

    write_helper.appendBuffer(set, Debug.tile_debug, .storage_buffer, tile_debug_buffer.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendImage(set, Debug.output, .storage_image, tile_debug_texture.default_view_handle, tile_debug_texture.image_layout);
}

// --------------------------------------------------------------------------
// Recording
// --------------------------------------------------------------------------

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: TiledLightingFrameGraphRecord,
    resources: *const TiledLightingPassResources,
    render_extent: vk.Extent2D,
    tile_extent: vk.Extent2D,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(resources.lighting.pipeline_index));

    const push_constants = hlsl_tiled_lighting.TiledLightingPushConstants{
        .extent_ts = .{ .x = render_extent.width, .y = render_extent.height },
        .extent_ts_inv = .{
            .x = 1.0 / @as(f32, @floatFromInt(render_extent.width)),
            .y = 1.0 / @as(f32, @floatFromInt(render_extent.height)),
        },
        .tile_count_x = tile_extent.width,
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        resources.lighting.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(@TypeOf(push_constants)),
        &push_constants,
    );

    const pass_descriptors = [_]vk.DescriptorSet{resources.tiled_lighting_descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.lighting.pipeline_layout, 0, &pass_descriptors, &.{});

    vkd.cmdDispatch(
        cmd_buffer,
        compute_helper.divRoundUp(render_extent.width, hlsl_tiled_lighting.TiledLightingThreadCountX),
        compute_helper.divRoundUp(render_extent.height, hlsl_tiled_lighting.TiledLightingThreadCountY),
        1,
    );
}

pub fn recordDebugCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: TiledLightingDebugFrameGraphRecord,
    resources: *const TiledLightingPassResources,
    render_extent: vk.Extent2D,
    tile_extent: vk.Extent2D,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(resources.debug.pipeline_index));

    const push_constants = hlsl_tiled_lighting.TiledLightingDebugPushConstants{
        .extent_ts = .{ .x = render_extent.width, .y = render_extent.height },
        .tile_count_x = tile_extent.width,
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        resources.debug.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(@TypeOf(push_constants)),
        &push_constants,
    );

    const pass_descriptors = [_]vk.DescriptorSet{resources.debug_descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.debug.pipeline_layout, 0, &pass_descriptors, &.{});

    vkd.cmdDispatch(
        cmd_buffer,
        compute_helper.divRoundUp(render_extent.width, hlsl_tiled_lighting.TiledLightingThreadCountX),
        compute_helper.divRoundUp(render_extent.height, hlsl_tiled_lighting.TiledLightingThreadCountY),
        1,
    );
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "descriptor bindings are dense and correctly ordered" {
    inline for (.{ Lighting.bindings, Debug.bindings }) |bindings| {
        for (bindings, 0..) |binding, i| {
            try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
        }
    }

    // Only the shadow map array is partially bound, and it is the binding the
    // C++ singles out by slot number.
    try testing.expectEqual(@as(u32, 9), Lighting.bindings[Lighting.shadow_map_array].slot);
    try testing.expectEqual(hlsl_mesh_instance.ShadowMapMaxCount, Lighting.bindings[Lighting.shadow_map_array].count);
}

test "the deferred output format matches the forward pass's HDR target" {
    // Both halves of the split screen feed the same tone mapping and swapchain
    // composite, so they have to agree on the HDR format.
    try testing.expectEqual(constants.forward_hdr_color_format, lighting_format);
}

test "the debug output is an 8-bit unorm the swapchain pass can sample" {
    try testing.expectEqual(vk.Format.r8g8b8a8_unorm, tile_debug_output_format);
}
