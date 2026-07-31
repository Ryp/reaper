// Port of src/renderer/vulkan/renderpass/TiledRasterPass.{h,cpp}
//
// Builds the per-tile light lists the tiled lighting pass consumes, in three
// steps:
//
//  1. Tile Depth Copy — blit HZB mip 3 into two tile-sized depth targets, one
//     holding the tile's min depth and one its max, and clear the light-list
//     and counter buffers.
//  2. Classify Light Volumes — a compute pass that sorts each light's proxy
//     volume into an "inner" or "outer" indirect draw list depending on whether
//     the camera is inside it.
//  3. Rasterize Light Volumes — draws both lists against the tile depth, with
//     the cull mode and depth compare flipped between them, and each covered
//     tile appends the light index to its list.

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
const gpu_texture_view = @import("../../texture/gpu_texture_view.zig");
const mesh_module = @import("../../../mesh/mesh.zig");
const obj_loader = @import("../../../mesh/obj_loader.zig");
const pipeline_module = @import("../pipeline.zig");
const pipeline_factory_module = @import("../pipeline_factory.zig");
const render_pass_helpers = @import("../render_pass_helpers.zig");
const shader_modules = @import("../shader_modules.zig");
const storage_buffer = @import("../storage_buffer.zig");
const tiled_lighting_common = @import("tiled_lighting_common.zig");

const hlsl = @import("../../hlsl/types.zig");
const hlsl_classify = @import("../../hlsl/tiled_lighting/classify_volume.zig");
const hlsl_copy_depth = @import("../../hlsl/copy_to_depth_from_hzb.zig");
const hlsl_tiled_lighting = @import("../../hlsl/tiled_lighting/tiled_lighting.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const framegraph_resources = @import("../framegraph_resources.zig");
const FrameGraphResources = framegraph_resources.FrameGraphResources;
const FrameGraphBuffer = framegraph_resources.FrameGraphBuffer;
const FrameGraphTexture = framegraph_resources.FrameGraphTexture;
const PipelineFactory = pipeline_factory_module.PipelineFactory;
const vma = @import("../vma.zig").c;

pub const tiled_raster_max_indirect_command_count: u32 = 512;

const max_vertex_count: u32 = 1024;

const proxy_mesh_path = "res/model/icosahedron.obj";

/// The HZB mip whose size matches the tile grid.
const tile_depth_hzb_mip: u32 = 3;

/// Two draws per frame: one for lights the camera is outside of and one for
/// lights it is inside.
const RasterPass = enum(u32) {
    inner = 0,
    outer = 1,

    const count = 2;
};

// --------------------------------------------------------------------------
// Descriptor bindings
// --------------------------------------------------------------------------

const Downsample = struct {
    const sampler = 0;
    const scene_depth = 1;
    const tile_depth_min = 2;
    const tile_depth_max = 3;

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .sampler, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 1, .count = 1, .type = .sampled_image, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 2, .count = 1, .type = .storage_image, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 3, .count = 1, .type = .storage_image, .stage_mask = .{ .compute_bit = true } },
    };
};

const Classify = struct {
    const vertex_positions_ms = 0;
    const inner_outer_counter = 1;
    const draw_commands_inner = 2;
    const draw_commands_outer = 3;
    const proxy_volume_buffer = 4;

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .storage_buffer, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 1, .count = 1, .type = .storage_buffer, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 2, .count = 1, .type = .storage_buffer, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 3, .count = 1, .type = .storage_buffer, .stage_mask = .{ .compute_bit = true } },
        .{ .slot = 4, .count = 1, .type = .storage_buffer, .stage_mask = .{ .compute_bit = true } },
    };
};

const Raster = struct {
    const light_volume_instances = 0;
    const tile_depth = 1;
    const tile_visible_light_indices = 2;
    const vertex_positions_ms = 3;

    const vertex_and_fragment = vk.ShaderStageFlags{ .vertex_bit = true, .fragment_bit = true };

    const bindings = [_]descriptor_set.DescriptorBinding{
        .{ .slot = 0, .count = 1, .type = .storage_buffer, .stage_mask = vertex_and_fragment },
        .{ .slot = 1, .count = 1, .type = .sampled_image, .stage_mask = vertex_and_fragment },
        .{ .slot = 2, .count = 1, .type = .storage_buffer, .stage_mask = vertex_and_fragment },
        .{ .slot = 3, .count = 1, .type = .storage_buffer, .stage_mask = .{ .vertex_bit = true } },
    };
};

// --------------------------------------------------------------------------
// Pipelines
// --------------------------------------------------------------------------

fn createDepthCopyPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("fullscreen_triangle.vert.spv"),
    );
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("copy_to_depth_from_hzb.frag.spv"),
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

fn createClassifyPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("tiled_lighting/classify_volume.comp.spv"),
    );

    const shader_stage = pipeline_module.defaultPipelineShaderStageCreateInfo(
        .{ .compute_bit = true },
        &module_create_info,
        null,
    );

    return pipeline_module.createComputePipeline(vkd, device, pipeline_layout, shader_stage);
}

fn createRasterPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("tiled_lighting/rasterize_light_volume.vert.spv"),
    );
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("tiled_lighting/rasterize_light_volume.frag.spv"),
    );

    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .fragment_bit = true }, &module_create_info_frag, null),
    };

    var properties = pipeline_module.defaultGraphicsPipelineProperties(null);
    properties.depth_stencil.depth_test_enable = .true;
    properties.depth_stencil.depth_write_enable = .false;
    properties.depth_stencil.depth_compare_op = .never; // dynamic
    properties.raster.cull_mode = .{ .front_bit = true, .back_bit = true }; // dynamic
    properties.input_assembly.topology = .triangle_list;
    properties.pipeline_layout = pipeline_layout;
    properties.pipeline_rendering.depth_attachment_format = constants.main_pass_depth_format;

    const dynamic_states = [_]vk.DynamicState{ .viewport, .scissor, .depth_compare_op, .cull_mode };

    return pipeline_module.createGraphicsPipeline(vkd, device, &shader_stages, &properties, &dynamic_states);
}

// --------------------------------------------------------------------------
// Resources
// --------------------------------------------------------------------------

pub const ProxyMeshAlloc = struct {
    vertex_offset: u32,
    vertex_count: u32,
};

pub const TiledRasterResources = struct {
    pub const DepthCopy = struct {
        descriptor_set_layout: vk.DescriptorSetLayout,
        pipeline_layout: vk.PipelineLayout,
        pipeline_index: u32,

        descriptor_set: vk.DescriptorSet,
    };

    pub const RasterResources = struct {
        descriptor_set_layout: vk.DescriptorSetLayout,
        pipeline_layout: vk.PipelineLayout,
        pipeline_index: u32,

        descriptor_sets: [RasterPass.count]vk.DescriptorSet,
    };

    pub const ClassifyResources = struct {
        descriptor_set_layout: vk.DescriptorSetLayout,
        pipeline_layout: vk.PipelineLayout,
        pipeline_index: u32,

        descriptor_set: vk.DescriptorSet,
    };

    depth_copy: DepthCopy,
    light_raster: RasterResources,
    classify: ClassifyResources,

    proxy_mesh_allocs: std.ArrayList(ProxyMeshAlloc) = .empty,
    vertex_buffer_offset: u32,
    vertex_buffer_position: buffer_module.GPUBuffer,

    allocator: std.mem.Allocator,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        vma_instance: vma.VmaAllocator,
        pipeline_factory: *PipelineFactory,
        allocator: std.mem.Allocator,
        io: std.Io,
    ) !TiledRasterResources {
        // ---- Depth copy ----
        const depth_copy_layout_bindings = [_]vk.DescriptorSetLayoutBinding{.{
            .binding = 0,
            .descriptor_type = .sampled_image,
            .descriptor_count = 1,
            .stage_flags = .{ .fragment_bit = true },
            .p_immutable_samplers = null,
        }};

        const depth_copy_set_layout = try pipeline_module.createDescriptorSetLayout(
            vkd,
            device,
            &depth_copy_layout_bindings,
            &[_]vk.DescriptorBindingFlags{.{}},
        );
        errdefer vkd.destroyDescriptorSetLayout(device, depth_copy_set_layout, null);

        const depth_copy_pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{depth_copy_set_layout},
            &[_]vk.PushConstantRange{.{
                .stage_flags = .{ .fragment_bit = true },
                .offset = 0,
                .size = @sizeOf(hlsl_copy_depth.CopyDepthFromHZBPushConstants),
            }},
        );
        errdefer vkd.destroyPipelineLayout(device, depth_copy_pipeline_layout, null);

        const depth_copy_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = depth_copy_pipeline_layout,
            .pipeline_creation_function = &createDepthCopyPipeline,
        });

        // ---- Raster ----
        var raster_layout_bindings: [Raster.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&raster_layout_bindings, &Raster.bindings);

        const raster_set_layout = try pipeline_module.createDescriptorSetLayout(
            vkd,
            device,
            &raster_layout_bindings,
            &([_]vk.DescriptorBindingFlags{.{}} ** Raster.bindings.len),
        );
        errdefer vkd.destroyDescriptorSetLayout(device, raster_set_layout, null);

        const raster_pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{raster_set_layout},
            &[_]vk.PushConstantRange{.{
                .stage_flags = Raster.vertex_and_fragment,
                .offset = 0,
                .size = @sizeOf(hlsl_tiled_lighting.TileLightRasterPushConstants),
            }},
        );
        errdefer vkd.destroyPipelineLayout(device, raster_pipeline_layout, null);

        const raster_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = raster_pipeline_layout,
            .pipeline_creation_function = &createRasterPipeline,
        });

        // ---- Classify ----
        var classify_layout_bindings: [Classify.bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&classify_layout_bindings, &Classify.bindings);

        const classify_set_layout = try pipeline_module.createDescriptorSetLayout(
            vkd,
            device,
            &classify_layout_bindings,
            &([_]vk.DescriptorBindingFlags{.{}} ** Classify.bindings.len),
        );
        errdefer vkd.destroyDescriptorSetLayout(device, classify_set_layout, null);

        const classify_pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{classify_set_layout},
            &[_]vk.PushConstantRange{.{
                .stage_flags = .{ .compute_bit = true },
                .offset = 0,
                .size = @sizeOf(hlsl_classify.ClassifyVolumePushConstants),
            }},
        );
        errdefer vkd.destroyPipelineLayout(device, classify_pipeline_layout, null);

        const classify_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = classify_pipeline_layout,
            .pipeline_creation_function = &createClassifyPipeline,
        });

        // The two raster sets are allocated from the same layout: one per
        // inner/outer pass, since they bind different tile depth targets.
        const set_layouts = [_]vk.DescriptorSetLayout{
            depth_copy_set_layout,
            classify_set_layout,
            raster_set_layout,
            raster_set_layout,
        };

        var sets: [set_layouts.len]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &set_layouts, &sets);

        // ---- Proxy volume geometry ----
        const properties = gpu_buffer.defaultBufferProperties(
            max_vertex_count,
            @sizeOf(hlsl.Float3),
            .{ .storage_buffer = true, .vertex_buffer = true },
        );

        const vertex_buffer_position = try buffer_module.createBuffer(vma_instance, properties, .cpu_to_gpu);
        errdefer buffer_module.destroyBuffer(vma_instance, vertex_buffer_position);

        var proxy_mesh_allocs: std.ArrayList(ProxyMeshAlloc) = .empty;
        errdefer proxy_mesh_allocs.deinit(allocator);

        const proxy_data = try std.Io.Dir.cwd().readFileAlloc(io, proxy_mesh_path, allocator, .limited(1 << 30));
        defer allocator.free(proxy_data);

        var icosahedron = try obj_loader.loadObjFromSlice(allocator, proxy_data);
        defer icosahedron.deinit(allocator);

        const icosahedron_alloc = ProxyMeshAlloc{
            .vertex_offset = 0,
            .vertex_count = @intCast(icosahedron.positions.items.len),
        };

        try proxy_mesh_allocs.append(allocator, icosahedron_alloc);

        // FIXME It's assumed here that the mesh indices are flat.
        try buffer_module.uploadBufferData(
            vma_instance,
            vertex_buffer_position,
            properties,
            std.mem.sliceAsBytes(icosahedron.positions.items),
            icosahedron_alloc.vertex_offset,
        );

        return .{
            .depth_copy = .{
                .descriptor_set_layout = depth_copy_set_layout,
                .pipeline_layout = depth_copy_pipeline_layout,
                .pipeline_index = depth_copy_pipeline_index,
                .descriptor_set = sets[0],
            },
            .classify = .{
                .descriptor_set_layout = classify_set_layout,
                .pipeline_layout = classify_pipeline_layout,
                .pipeline_index = classify_pipeline_index,
                .descriptor_set = sets[1],
            },
            .light_raster = .{
                .descriptor_set_layout = raster_set_layout,
                .pipeline_layout = raster_pipeline_layout,
                .pipeline_index = raster_pipeline_index,
                .descriptor_sets = .{ sets[2], sets[3] },
            },
            .proxy_mesh_allocs = proxy_mesh_allocs,
            .vertex_buffer_offset = icosahedron_alloc.vertex_count,
            .vertex_buffer_position = vertex_buffer_position,
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *TiledRasterResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        buffer_module.destroyBuffer(vma_instance, self.vertex_buffer_position);
        self.proxy_mesh_allocs.deinit(self.allocator);

        vkd.destroyPipelineLayout(device, self.depth_copy.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.depth_copy.descriptor_set_layout, null);

        vkd.destroyPipelineLayout(device, self.light_raster.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.light_raster.descriptor_set_layout, null);

        vkd.destroyPipelineLayout(device, self.classify.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.classify.descriptor_set_layout, null);
    }
};

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const LightRasterFrameGraphRecord = struct {
    pub const TileDepthCopy = struct {
        pass_handle: fg.RenderPassHandle,
        depth_min: fg.ResourceUsageHandle,
        depth_max: fg.ResourceUsageHandle,
        hzb_texture: fg.ResourceUsageHandle,
        light_list_clear: fg.ResourceUsageHandle,
        classification_counters_clear: fg.ResourceUsageHandle,
    };

    pub const ClassifyRecord = struct {
        pass_handle: fg.RenderPassHandle,
        classification_counters: fg.ResourceUsageHandle,
        draw_commands_inner: fg.ResourceUsageHandle,
        draw_commands_outer: fg.ResourceUsageHandle,
    };

    pub const RasterRecord = struct {
        pass_handle: fg.RenderPassHandle,
        command_counters: fg.ResourceUsageHandle,
        draw_commands_inner: fg.ResourceUsageHandle,
        draw_commands_outer: fg.ResourceUsageHandle,
        tile_depth_min: fg.ResourceUsageHandle,
        tile_depth_max: fg.ResourceUsageHandle,
        light_list: fg.ResourceUsageHandle,
    };

    tile_depth_properties: gpu_texture_properties.GPUTextureProperties,
    tile_depth_copy: TileDepthCopy,
    light_classify: ClassifyRecord,
    light_raster: RasterRecord,
};

pub fn createFrameGraphRecord(
    builder: *Builder,
    tiled_lighting_frame: *const tiled_lighting_common.TiledLightingFrame,
    hzb_properties: gpu_texture_properties.GPUTextureProperties,
    hzb_usage_handle: fg.ResourceUsageHandle,
) !LightRasterFrameGraphRecord {
    var record: LightRasterFrameGraphRecord = undefined;

    const tile_depth_copy = &record.tile_depth_copy;

    tile_depth_copy.pass_handle = try builder.createRenderPass("Tile Depth Copy", false);

    const tile_depth_properties = gpu_texture_properties.defaultTextureProperties(
        tiled_lighting_frame.tile_count_x,
        tiled_lighting_frame.tile_count_y,
        constants.main_pass_depth_format,
        .{ .depth_stencil_attachment = true, .sampled = true },
    );

    record.tile_depth_properties = tile_depth_properties;

    const tile_depth_copy_dst_access = barrier_module.GPUTextureAccess{
        .stage_mask = .{ .early_fragment_tests_bit = true, .late_fragment_tests_bit = true },
        .access_mask = .{ .depth_stencil_attachment_write_bit = true },
        .image_layout = .attachment_optimal,
    };

    tile_depth_copy.depth_min = try builder.createTexture(
        tile_depth_copy.pass_handle,
        "Tile Depth Min",
        tile_depth_properties,
        tile_depth_copy_dst_access,
        &.{},
    );
    tile_depth_copy.depth_max = try builder.createTexture(
        tile_depth_copy.pass_handle,
        "Tile Depth Max",
        tile_depth_properties,
        tile_depth_copy_dst_access,
        &.{},
    );

    {
        var hzb_view = gpu_texture_view.defaultTextureView(hzb_properties);
        hzb_view.subresource.mip_count = 1;
        hzb_view.subresource.mip_offset = tile_depth_hzb_mip;

        std.debug.assert(tile_depth_properties.width == hzb_properties.width >> @intCast(tile_depth_hzb_mip));
        std.debug.assert(tile_depth_properties.height == hzb_properties.height >> @intCast(tile_depth_hzb_mip));

        tile_depth_copy.hzb_texture = try builder.readTexture(
            tile_depth_copy.pass_handle,
            hzb_usage_handle,
            .{
                .stage_mask = .{ .fragment_shader_bit = true },
                .access_mask = .{ .shader_read_bit = true },
                .image_layout = .read_only_optimal,
            },
            &.{hzb_view},
        );
    }

    tile_depth_copy.light_list_clear = try builder.createBuffer(
        tile_depth_copy.pass_handle,
        "Light lists",
        gpu_buffer.defaultBufferProperties(
            hlsl_tiled_lighting.ElementsPerTile * tile_depth_properties.width * tile_depth_properties.height,
            @sizeOf(u32),
            .{ .storage_buffer = true, .transfer_dst = true },
        ),
        .{ .stage_mask = .{ .all_transfer_bit = true }, .access_mask = .{ .transfer_write_bit = true } },
        &.{},
    );

    const classification_counters_properties = gpu_buffer.defaultBufferProperties(
        RasterPass.count,
        @sizeOf(u32),
        .{ .storage_buffer = true, .transfer_dst = true, .indirect_buffer = true },
    );

    tile_depth_copy.classification_counters_clear = try builder.createBuffer(
        tile_depth_copy.pass_handle,
        "Classification counters",
        classification_counters_properties,
        .{ .stage_mask = .{ .all_transfer_bit = true }, .access_mask = .{ .transfer_write_bit = true } },
        &.{},
    );

    const light_classify = &record.light_classify;

    light_classify.pass_handle = try builder.createRenderPass("Classify Light Volumes", false);

    light_classify.classification_counters = try builder.writeBuffer(
        light_classify.pass_handle,
        tile_depth_copy.classification_counters_clear,
        .{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_write_bit = true, .shader_read_bit = true },
        },
        &.{},
    );

    const draw_command_classify_properties = gpu_buffer.defaultBufferProperties(
        tiled_raster_max_indirect_command_count,
        4 * @sizeOf(u32), // FIXME
        .{ .storage_buffer = true, .indirect_buffer = true },
    );

    const draw_command_classify_access = barrier_module.GPUBufferAccess{
        .stage_mask = .{ .compute_shader_bit = true },
        .access_mask = .{ .shader_write_bit = true },
    };

    light_classify.draw_commands_inner = try builder.createBuffer(
        light_classify.pass_handle,
        "Draw Commands Inner",
        draw_command_classify_properties,
        draw_command_classify_access,
        &.{},
    );
    light_classify.draw_commands_outer = try builder.createBuffer(
        light_classify.pass_handle,
        "Draw Commands Outer",
        draw_command_classify_properties,
        draw_command_classify_access,
        &.{},
    );

    const light_raster = &record.light_raster;

    light_raster.pass_handle = try builder.createRenderPass("Rasterize Light Volumes", false);

    light_raster.command_counters = try builder.readBuffer(
        light_raster.pass_handle,
        light_classify.classification_counters,
        .{ .stage_mask = .{ .draw_indirect_bit = true }, .access_mask = .{ .indirect_command_read_bit = true } },
        &.{},
    );

    {
        const draw_command_raster_read_access = barrier_module.GPUBufferAccess{
            .stage_mask = .{ .draw_indirect_bit = true },
            .access_mask = .{ .indirect_command_read_bit = true },
        };

        light_raster.draw_commands_inner = try builder.readBuffer(
            light_raster.pass_handle,
            light_classify.draw_commands_inner,
            draw_command_raster_read_access,
            &.{},
        );
        light_raster.draw_commands_outer = try builder.readBuffer(
            light_raster.pass_handle,
            light_classify.draw_commands_outer,
            draw_command_raster_read_access,
            &.{},
        );
    }

    {
        const tile_depth_access = barrier_module.GPUTextureAccess{
            .stage_mask = .{ .early_fragment_tests_bit = true, .late_fragment_tests_bit = true },
            .access_mask = .{ .depth_stencil_attachment_read_bit = true },
            .image_layout = .depth_read_only_optimal,
        };

        light_raster.tile_depth_min = try builder.readTexture(
            light_raster.pass_handle,
            tile_depth_copy.depth_min,
            tile_depth_access,
            &.{},
        );
        light_raster.tile_depth_max = try builder.readTexture(
            light_raster.pass_handle,
            tile_depth_copy.depth_max,
            tile_depth_access,
            &.{},
        );
    }

    light_raster.light_list = try builder.writeBuffer(
        light_raster.pass_handle,
        tile_depth_copy.light_list_clear,
        .{
            .stage_mask = .{ .fragment_shader_bit = true },
            .access_mask = .{ .shader_write_bit = true, .shader_read_bit = true },
        },
        &.{},
    );

    return record;
}

// --------------------------------------------------------------------------
// Descriptor updates
// --------------------------------------------------------------------------

pub fn updateDescriptorSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: LightRasterFrameGraphRecord,
    frame_storage_allocator: *storage_buffer.StorageBufferAllocator,
    resources: *const TiledRasterResources,
    tiled_lighting_frame: *const tiled_lighting_common.TiledLightingFrame,
) void {
    if (tiled_lighting_frame.light_volumes.items.len == 0) return;

    {
        // The depth copy reads the tile-sized HZB mip, which is the only view
        // the frame graph was asked to create for this usage.
        const hzb_texture = frame_graph_resources.getTexture(framegraph, record.tile_depth_copy.hzb_texture);

        write_helper.appendImage(
            resources.depth_copy.descriptor_set,
            0,
            .sampled_image,
            hzb_texture.additional_views[0],
            hzb_texture.image_layout,
        );
    }

    const proxy_volumes_alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_tiled_lighting.ProxyVolumeInstance,
        tiled_lighting_frame.proxy_volumes.items,
    );

    {
        const set = resources.classify.descriptor_set;

        const classification_counters = frame_graph_resources.getBuffer(
            framegraph,
            record.light_classify.classification_counters,
        );
        const draw_commands_inner = frame_graph_resources.getBuffer(framegraph, record.light_classify.draw_commands_inner);
        const draw_commands_outer = frame_graph_resources.getBuffer(framegraph, record.light_classify.draw_commands_outer);

        write_helper.appendBuffer(set, Classify.vertex_positions_ms, .storage_buffer, resources.vertex_buffer_position.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(set, Classify.inner_outer_counter, .storage_buffer, classification_counters.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(set, Classify.draw_commands_inner, .storage_buffer, draw_commands_inner.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(set, Classify.draw_commands_outer, .storage_buffer, draw_commands_outer.handle, 0, vk.WHOLE_SIZE);
        write_helper.appendBuffer(
            set,
            Classify.proxy_volume_buffer,
            .storage_buffer,
            proxy_volumes_alloc.buffer,
            proxy_volumes_alloc.offset_bytes,
            proxy_volumes_alloc.size_bytes,
        );
    }

    const light_volumes_alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_tiled_lighting.LightVolumeInstance,
        tiled_lighting_frame.light_volumes.items,
    );

    {
        const depth_min = frame_graph_resources.getTexture(framegraph, record.light_raster.tile_depth_min);
        const depth_max = frame_graph_resources.getTexture(framegraph, record.light_raster.tile_depth_max);
        const light_list_buffer = frame_graph_resources.getBuffer(framegraph, record.light_raster.light_list);

        // NOTE: inner samples the MIN depth and outer the MAX — the opposite of
        // which target each pass renders into. Both halves come straight from
        // the C++.
        const tile_depths = [RasterPass.count]FrameGraphTexture{ depth_min, depth_max };

        for (resources.light_raster.descriptor_sets, tile_depths) |set, tile_depth| {
            write_helper.appendBuffer(
                set,
                Raster.light_volume_instances,
                .storage_buffer,
                light_volumes_alloc.buffer,
                light_volumes_alloc.offset_bytes,
                light_volumes_alloc.size_bytes,
            );
            write_helper.appendImage(set, Raster.tile_depth, .sampled_image, tile_depth.default_view_handle, tile_depth.image_layout);
            write_helper.appendBuffer(set, Raster.tile_visible_light_indices, .storage_buffer, light_list_buffer.handle, 0, vk.WHOLE_SIZE);
            write_helper.appendBuffer(set, Raster.vertex_positions_ms, .storage_buffer, resources.vertex_buffer_position.handle, 0, vk.WHOLE_SIZE);
        }
    }
}

// --------------------------------------------------------------------------
// Recording
// --------------------------------------------------------------------------

pub fn recordDepthCopy(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: LightRasterFrameGraphRecord.TileDepthCopy,
    resources: *const TiledRasterResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const depth_dsts = [_]FrameGraphTexture{
        helper.resources.getTexture(helper.frame_graph, record.depth_min),
        helper.resources.getTexture(helper.frame_graph, record.depth_max),
    };

    const depth_extent = vk.Extent2D{
        .width = depth_dsts[0].properties.width,
        .height = depth_dsts[0].properties.height,
    };
    const pass_rect = render_pass_helpers.defaultRect(depth_extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(resources.depth_copy.pipeline_index));

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    const pass_descriptors = [_]vk.DescriptorSet{resources.depth_copy.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .graphics, resources.depth_copy.pipeline_layout, 0, &pass_descriptors, &.{});

    for (depth_dsts, 0..) |depth_dst, depth_index| {
        var depth_attachment = pipeline_module.defaultRenderingAttachmentInfo(
            depth_dst.default_view_handle,
            depth_dst.image_layout,
        );
        depth_attachment.load_op = .dont_care;

        const rendering_info = pipeline_module.defaultRenderingInfo(pass_rect, &.{}, &depth_attachment);

        vkd.cmdBeginRendering(cmd_buffer, &rendering_info);

        const push_constants = hlsl_copy_depth.CopyDepthFromHZBPushConstants{
            .copy_min = if (depth_index == 0) 1 else 0,
        };

        vkd.cmdPushConstants(
            cmd_buffer,
            resources.depth_copy.pipeline_layout,
            .{ .fragment_bit = true },
            0,
            @sizeOf(@TypeOf(push_constants)),
            &push_constants,
        );

        vkd.cmdDraw(cmd_buffer, 3, 1, 0, 0);

        vkd.cmdEndRendering(cmd_buffer);
    }

    // Clear buffers
    {
        const clear_value: u32 = 0;

        const light_lists = helper.resources.getBuffer(helper.frame_graph, record.light_list_clear);
        vkd.cmdFillBuffer(
            cmd_buffer,
            light_lists.handle,
            light_lists.default_view.offset_bytes,
            light_lists.default_view.size_bytes,
            clear_value,
        );

        const counters = helper.resources.getBuffer(helper.frame_graph, record.classification_counters_clear);
        vkd.cmdFillBuffer(
            cmd_buffer,
            counters.handle,
            counters.default_view.offset_bytes,
            counters.default_view.size_bytes,
            clear_value,
        );
    }
}

pub fn recordLightClassifyCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: LightRasterFrameGraphRecord.ClassifyRecord,
    tiled_lighting_frame: *const tiled_lighting_common.TiledLightingFrame,
    resources: *const TiledRasterResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const icosahedron_alloc = resources.proxy_mesh_allocs.items[0]; // FIXME

    const light_volumes_count: u32 = @intCast(tiled_lighting_frame.light_volumes.items.len);

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(resources.classify.pipeline_index));

    const push_constants = hlsl_classify.ClassifyVolumePushConstants{
        .vertex_offset = icosahedron_alloc.vertex_offset,
        .vertex_count = icosahedron_alloc.vertex_count,
        .instance_id_offset = 0, // FIXME
        .near_clip_plane_depth_vs = 0.1, // FIXME copied from near_plane_distance
    };

    vkd.cmdPushConstants(
        cmd_buffer,
        resources.classify.pipeline_layout,
        .{ .compute_bit = true },
        0,
        @sizeOf(@TypeOf(push_constants)),
        &push_constants,
    );

    const pass_descriptors = [_]vk.DescriptorSet{resources.classify.descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.classify.pipeline_layout, 0, &pass_descriptors, &.{});

    vkd.cmdDispatch(
        cmd_buffer,
        compute_helper.divRoundUp(icosahedron_alloc.vertex_count, hlsl_classify.ClassifyVolumeThreadCount),
        light_volumes_count,
        1,
    );
}

pub fn recordLightRasterCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: LightRasterFrameGraphRecord.RasterRecord,
    resources: TiledRasterResources.RasterResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const command_counters = helper.resources.getBuffer(helper.frame_graph, record.command_counters);

    // NOTE: inner renders into the MAX depth target and outer into the MIN,
    // which is the opposite of what each set samples. Verbatim from the C++.
    const depth_buffers = [RasterPass.count]FrameGraphTexture{
        helper.resources.getTexture(helper.frame_graph, record.tile_depth_max),
        helper.resources.getTexture(helper.frame_graph, record.tile_depth_min),
    };

    const draw_commands = [RasterPass.count]FrameGraphBuffer{
        helper.resources.getBuffer(helper.frame_graph, record.draw_commands_inner),
        helper.resources.getBuffer(helper.frame_graph, record.draw_commands_outer),
    };

    const depth_extent = vk.Extent2D{
        .width = depth_buffers[0].properties.width,
        .height = depth_buffers[0].properties.height,
    };
    const pass_rect = render_pass_helpers.defaultRect(depth_extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(resources.pipeline_index));

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    for (0..RasterPass.count) |pass_index| {
        const is_inner = pass_index == @intFromEnum(RasterPass.inner);

        const depth_buffer = depth_buffers[pass_index];

        var depth_attachment = pipeline_module.defaultRenderingAttachmentInfo(
            depth_buffer.default_view_handle,
            depth_buffer.image_layout,
        );
        depth_attachment.store_op = .dont_care;

        const rendering_info = pipeline_module.defaultRenderingInfo(pass_rect, &.{}, &depth_attachment);

        // Inside the volume, the front faces are behind the camera, so the two
        // passes flip both the cull mode and the depth test.
        vkd.cmdSetCullMode(cmd_buffer, if (is_inner)
            .{ .front_bit = true }
        else
            .{ .back_bit = true });
        vkd.cmdSetDepthCompareOp(cmd_buffer, if (is_inner) .less_or_equal else .greater_or_equal);

        vkd.cmdBeginRendering(cmd_buffer, &rendering_info);

        const pass_descriptors = [_]vk.DescriptorSet{resources.descriptor_sets[pass_index]};
        vkd.cmdBindDescriptorSets(cmd_buffer, .graphics, resources.pipeline_layout, 0, &pass_descriptors, &.{});

        const push_constants = hlsl_tiled_lighting.TileLightRasterPushConstants{
            .instance_id_offset = 0, // FIXME
            .tile_count_x = depth_extent.width,
        };

        vkd.cmdPushConstants(
            cmd_buffer,
            resources.pipeline_layout,
            Raster.vertex_and_fragment,
            0,
            @sizeOf(@TypeOf(push_constants)),
            &push_constants,
        );

        const command_buffer_offset: u64 = 0;
        const counter_buffer_offset: u64 = pass_index * @sizeOf(u32);

        vkd.cmdDrawIndirectCount(
            cmd_buffer,
            draw_commands[pass_index].handle,
            command_buffer_offset,
            command_counters.handle,
            counter_buffer_offset,
            tiled_raster_max_indirect_command_count,
            draw_commands[pass_index].properties.element_size_bytes,
        );

        vkd.cmdEndRendering(cmd_buffer);
    }
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "descriptor bindings are dense and correctly ordered" {
    inline for (.{ Downsample.bindings, Classify.bindings, Raster.bindings }) |bindings| {
        for (bindings, 0..) |binding, i| {
            try testing.expectEqual(@as(u32, @intCast(i)), binding.slot);
        }
    }
}

test "the classification counter buffer has one slot per raster pass" {
    // The raster pass reads its indirect count at `pass_index * sizeof(u32)`,
    // so the buffer has to be exactly as long as there are passes.
    try testing.expectEqual(@as(u32, 2), RasterPass.count);
    try testing.expectEqual(@as(u32, 0), @intFromEnum(RasterPass.inner));
    try testing.expectEqual(@as(u32, 1), @intFromEnum(RasterPass.outer));
}

test "the icosahedron fits in the proxy vertex buffer" {
    // res/model/icosahedron.obj is flattened to one vertex per index, so the
    // budget is on triangles, not unique vertices.
    try testing.expect(max_vertex_count >= 20 * 3);
}

test "the tile depth mip is the one whose size matches the tile grid" {
    // The HZB is tile_count * 8 across and has 4 mips; mip 3 is therefore
    // exactly tile_count. createFrameGraphRecord asserts this at runtime too.
    try testing.expectEqual(@as(u32, 3), tile_depth_hzb_mip);
    try testing.expectEqual(@as(u32, 1), @as(u32, 8) >> @intCast(tile_depth_hzb_mip));
}
