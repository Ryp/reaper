// Port of src/renderer/vulkan/renderpass/DebugGeometryRenderPass.{h,cpp}
//
// Wireframe proxy volumes (a sphere and a box) drawn over the lit scene. Three
// passes: "start" uploads the CPU-side commands and seeds the draw counter, a
// compute pass expands each command into an indirect draw plus an instance, and
// the draw pass runs a single indexed indirect-count draw over the result.
//
// Nothing emits commands in v1 — the physics sim that produces them is post-v1
// — so the counter is normally zero and the draw is a no-op. The passes are
// still declared and recorded, because that is what the C++ does and because a
// zero-count indirect draw is the only way to prove the plumbing is right.

const std = @import("std");
const vk = @import("vulkan");

const buffer_module = @import("../buffer.zig");
const constants = @import("constants.zig");
const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_buffer = @import("../../buffer/gpu_buffer.zig");
const obj_loader = @import("../../../mesh/obj_loader.zig");
const pipeline_module = @import("../pipeline.zig");
const render_pass_helpers = @import("../render_pass_helpers.zig");
const shader_modules = @import("../shader_modules.zig");
const divRoundUp = @import("../compute_helper.zig").divRoundUp;

const hlsl = @import("../../hlsl/types.zig");
const hlsl_private = @import("../../hlsl/debug_geometry/debug_geometry_private.zig");
const hlsl_public = @import("../../hlsl/debug_geometry/debug_geometry_public.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const PipelineFactory = @import("../pipeline_factory.zig").PipelineFactory;
const prepare_buckets = @import("../../prepare_buckets.zig");
const vma = @import("../vma.zig").c;

pub const debug_geometry_count_max: u32 = 2048;

const max_cpu_debug_command_count: u32 = 1024;
const max_index_count: u32 = 1024;
const max_vertex_count: u32 = max_index_count * 3;

/// Indexed by DebugGeometryType_*; the order has to match the shader's enum.
const proxy_mesh_paths = [hlsl_public.DebugGeometryTypeCount][]const u8{
    "res/model/icosahedron.obj",
    "res/model/box.obj",
};

pub const DebugMeshAlloc = struct {
    index_offset: u32,
    index_count: u32,
    vertex_offset: u32,
    vertex_count: u32,
};

// --------------------------------------------------------------------------
// Pipelines
// --------------------------------------------------------------------------

fn createBuildCmdsPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("debug_geometry/build_cmds.comp.spv"),
    );

    return pipeline_module.createComputePipeline(
        vkd,
        device,
        pipeline_layout,
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .compute_bit = true }, &module_create_info, null),
    );
}

fn createDrawPipeline(
    vkd: *const vk.DeviceWrapper,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
) anyerror!vk.Pipeline {
    const module_create_info_vert = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("debug_geometry/draw.vert.spv"),
    );
    const module_create_info_frag = pipeline_module.shaderModuleCreateInfo(
        shader_modules.get("debug_geometry/draw.frag.spv"),
    );

    const shader_stages = [_]vk.PipelineShaderStageCreateInfo{
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .vertex_bit = true }, &module_create_info_vert, null),
        pipeline_module.defaultPipelineShaderStageCreateInfo(.{ .fragment_bit = true }, &module_create_info_frag, null),
    };

    const blend_attachment_states = [_]vk.PipelineColorBlendAttachmentState{
        pipeline_module.defaultPipelineColorBlendAttachmentState(),
    };

    const color_formats = [_]vk.Format{constants.forward_hdr_color_format};

    var properties = pipeline_module.defaultGraphicsPipelineProperties(null);

    // Depth tested against the scene but never written: the wireframe has to
    // hide behind geometry without occluding anything drawn after it.
    properties.depth_stencil.depth_test_enable = .true;
    properties.depth_stencil.depth_write_enable = .false;
    properties.depth_stencil.depth_compare_op = if (constants.main_pass_use_reverse_z) .greater else .less;
    properties.raster.polygon_mode = .line;
    properties.raster.cull_mode = .{};
    properties.input_assembly.topology = .triangle_list;
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

pub const DebugGeometryPassResources = struct {
    pub const Stage = struct {
        descriptor_set_layout: vk.DescriptorSetLayout,
        pipeline_layout: vk.PipelineLayout,
        pipeline_index: u32,
    };

    build_cmds: Stage,
    build_cmds_descriptor_set: vk.DescriptorSet,

    draw: Stage,
    draw_descriptor_set: vk.DescriptorSet,

    cpu_commands_staging_buffer: buffer_module.GPUBuffer,
    cpu_commands_staging_properties: gpu_buffer.GPUBufferProperties,
    build_cmds_constants: buffer_module.GPUBuffer,
    build_cmds_constants_properties: gpu_buffer.GPUBufferProperties,

    proxy_mesh_allocs: [hlsl_public.DebugGeometryTypeCount]DebugMeshAlloc,

    index_buffer: buffer_module.GPUBuffer,
    index_buffer_properties: gpu_buffer.GPUBufferProperties,
    vertex_buffer_position: buffer_module.GPUBuffer,
    vertex_buffer_properties: gpu_buffer.GPUBufferProperties,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        vma_instance: vma.VmaAllocator,
        pipeline_factory: *PipelineFactory,
        allocator: std.mem.Allocator,
        io: std.Io,
    ) !DebugGeometryPassResources {
        const build_cmds_bindings = [_]vk.DescriptorSetLayoutBinding{
            .{ .binding = 0, .descriptor_type = .uniform_buffer, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 1, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 2, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 3, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 4, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .compute_bit = true }, .p_immutable_samplers = null },
        };

        const draw_bindings = [_]vk.DescriptorSetLayoutBinding{
            .{ .binding = 0, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .vertex_bit = true }, .p_immutable_samplers = null },
            .{ .binding = 1, .descriptor_type = .storage_buffer, .descriptor_count = 1, .stage_flags = .{ .vertex_bit = true }, .p_immutable_samplers = null },
        };

        const build_cmds_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &build_cmds_bindings, &.{});
        errdefer vkd.destroyDescriptorSetLayout(device, build_cmds_layout, null);

        const build_cmds_pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &.{build_cmds_layout}, &.{});
        errdefer vkd.destroyPipelineLayout(device, build_cmds_pipeline_layout, null);

        const build_cmds_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = build_cmds_pipeline_layout,
            .pipeline_creation_function = &createBuildCmdsPipeline,
        });

        const draw_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, &draw_bindings, &.{});
        errdefer vkd.destroyDescriptorSetLayout(device, draw_layout, null);

        const draw_pipeline_layout = try pipeline_module.createPipelineLayout(vkd, device, &.{draw_layout}, &.{});
        errdefer vkd.destroyPipelineLayout(device, draw_pipeline_layout, null);

        const draw_pipeline_index = try pipeline_factory.registerPipelineCreator(.{
            .pipeline_layout = draw_pipeline_layout,
            .pipeline_creation_function = &createDrawPipeline,
        });

        const set_layouts = [_]vk.DescriptorSetLayout{ build_cmds_layout, draw_layout };
        var sets: [set_layouts.len]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &set_layouts, &sets);

        // ---- Buffers ----
        const cpu_commands_staging_properties = gpu_buffer.defaultBufferProperties(
            max_cpu_debug_command_count,
            @sizeOf(hlsl_public.DebugGeometryUserCommand),
            .{ .transfer_src = true },
        );
        const cpu_commands_staging_buffer = try buffer_module.createBuffer(
            vma_instance,
            cpu_commands_staging_properties,
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, cpu_commands_staging_buffer);

        const build_cmds_constants_properties = gpu_buffer.defaultBufferProperties(
            1,
            @sizeOf(hlsl_private.DebugGeometryBuildCmdsPassConstants),
            .{ .uniform_buffer = true },
        );
        const build_cmds_constants = try buffer_module.createBuffer(
            vma_instance,
            build_cmds_constants_properties,
            .cpu_to_gpu,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, build_cmds_constants);

        const index_buffer_properties = gpu_buffer.defaultBufferProperties(
            max_index_count,
            @sizeOf(hlsl.Uint),
            .{ .storage_buffer = true, .index_buffer = true },
        );
        const index_buffer = try buffer_module.createBuffer(vma_instance, index_buffer_properties, .cpu_to_gpu);
        errdefer buffer_module.destroyBuffer(vma_instance, index_buffer);

        const vertex_buffer_properties = gpu_buffer.defaultBufferProperties(
            max_vertex_count,
            @sizeOf(hlsl.Float3),
            .{ .storage_buffer = true },
        );
        const vertex_buffer_position = try buffer_module.createBuffer(vma_instance, vertex_buffer_properties, .cpu_to_gpu);
        errdefer buffer_module.destroyBuffer(vma_instance, vertex_buffer_position);

        // ---- Proxy meshes ----
        var proxy_mesh_allocs: [hlsl_public.DebugGeometryTypeCount]DebugMeshAlloc = undefined;

        var index_buffer_offset: u32 = 0;
        var vertex_buffer_offset: u32 = 0;

        for (proxy_mesh_paths, 0..) |path, i| {
            const obj_data = try std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(1 << 30));
            defer allocator.free(obj_data);

            var mesh = try obj_loader.loadObjFromSlice(allocator, obj_data);
            defer mesh.deinit(allocator);

            const alloc = DebugMeshAlloc{
                .index_offset = index_buffer_offset,
                .index_count = @intCast(mesh.indexes.items.len),
                .vertex_offset = vertex_buffer_offset,
                .vertex_count = @intCast(mesh.positions.items.len),
            };

            index_buffer_offset += alloc.index_count;
            vertex_buffer_offset += alloc.vertex_count;

            try buffer_module.uploadBufferData(
                vma_instance,
                index_buffer,
                index_buffer_properties,
                std.mem.sliceAsBytes(mesh.indexes.items),
                alloc.index_offset,
            );

            try buffer_module.uploadBufferData(
                vma_instance,
                vertex_buffer_position,
                vertex_buffer_properties,
                std.mem.sliceAsBytes(mesh.positions.items),
                alloc.vertex_offset,
            );

            proxy_mesh_allocs[i] = alloc;
        }

        return .{
            .build_cmds = .{
                .descriptor_set_layout = build_cmds_layout,
                .pipeline_layout = build_cmds_pipeline_layout,
                .pipeline_index = build_cmds_pipeline_index,
            },
            .build_cmds_descriptor_set = sets[0],
            .draw = .{
                .descriptor_set_layout = draw_layout,
                .pipeline_layout = draw_pipeline_layout,
                .pipeline_index = draw_pipeline_index,
            },
            .draw_descriptor_set = sets[1],
            .cpu_commands_staging_buffer = cpu_commands_staging_buffer,
            .cpu_commands_staging_properties = cpu_commands_staging_properties,
            .build_cmds_constants = build_cmds_constants,
            .build_cmds_constants_properties = build_cmds_constants_properties,
            .proxy_mesh_allocs = proxy_mesh_allocs,
            .index_buffer = index_buffer,
            .index_buffer_properties = index_buffer_properties,
            .vertex_buffer_position = vertex_buffer_position,
            .vertex_buffer_properties = vertex_buffer_properties,
        };
    }

    pub fn deinit(self: *DebugGeometryPassResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        buffer_module.destroyBuffer(vma_instance, self.vertex_buffer_position);
        buffer_module.destroyBuffer(vma_instance, self.index_buffer);
        buffer_module.destroyBuffer(vma_instance, self.build_cmds_constants);
        buffer_module.destroyBuffer(vma_instance, self.cpu_commands_staging_buffer);

        vkd.destroyPipelineLayout(device, self.draw.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.draw.descriptor_set_layout, null);

        vkd.destroyPipelineLayout(device, self.build_cmds.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.build_cmds.descriptor_set_layout, null);
    }
};

// --------------------------------------------------------------------------
// Frame graph records
// --------------------------------------------------------------------------

pub const StartFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    draw_counter: fg.ResourceUsageHandle,
    user_commands_buffer: fg.ResourceUsageHandle,
};

pub const ComputeFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    draw_counter: fg.ResourceUsageHandle,
    user_commands_buffer: fg.ResourceUsageHandle,
    draw_commands: fg.ResourceUsageHandle,
    instance_buffer: fg.ResourceUsageHandle,
};

pub const DrawFrameGraphRecord = struct {
    pass_handle: fg.RenderPassHandle,
    scene_hdr: fg.ResourceUsageHandle,
    scene_depth: fg.ResourceUsageHandle,
    draw_counter: fg.ResourceUsageHandle,
    draw_commands: fg.ResourceUsageHandle,
    instance_buffer: fg.ResourceUsageHandle,
};

const transfer_write = fg.GPUResourceAccess{
    .stage_mask = .{ .clear_bit = true },
    .access_mask = .{ .transfer_write_bit = true },
    .image_layout = .undefined,
};

const compute_read = fg.GPUResourceAccess{
    .stage_mask = .{ .compute_shader_bit = true },
    .access_mask = .{ .shader_read_bit = true },
    .image_layout = .undefined,
};

const compute_write = fg.GPUResourceAccess{
    .stage_mask = .{ .compute_shader_bit = true },
    .access_mask = .{ .shader_write_bit = true },
    .image_layout = .undefined,
};

const indirect_read = fg.GPUResourceAccess{
    .stage_mask = .{ .draw_indirect_bit = true },
    .access_mask = .{ .indirect_command_read_bit = true },
    .image_layout = .undefined,
};

pub fn createStartFrameGraphRecord(builder: *Builder) !StartFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Debug Geometry Start", false);

    const draw_counter = try builder.createBuffer(
        pass_handle,
        "Debug Indirect draw counter buffer",
        gpu_buffer.defaultBufferProperties(1, @sizeOf(u32), .{
            .indirect_buffer = true,
            .storage_buffer = true,
            .transfer_dst = true,
        }),
        fg.toBufferAccess(transfer_write),
        &.{},
    );

    const user_commands_buffer = try builder.createBuffer(
        pass_handle,
        "Debug geometry user command buffer",
        gpu_buffer.defaultBufferProperties(
            debug_geometry_count_max,
            @sizeOf(hlsl_public.DebugGeometryUserCommand),
            .{ .storage_buffer = true, .transfer_dst = true },
        ),
        fg.toBufferAccess(transfer_write),
        &.{},
    );

    return .{
        .pass_handle = pass_handle,
        .draw_counter = draw_counter,
        .user_commands_buffer = user_commands_buffer,
    };
}

pub fn createComputeFrameGraphRecord(
    builder: *Builder,
    draw_counter_handle: fg.ResourceUsageHandle,
    user_commands_buffer_handle: fg.ResourceUsageHandle,
) !ComputeFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Debug Geometry Build Commands", false);

    const draw_counter = try builder.readBuffer(pass_handle, draw_counter_handle, fg.toBufferAccess(compute_read), &.{});
    const user_commands_buffer = try builder.readBuffer(
        pass_handle,
        user_commands_buffer_handle,
        fg.toBufferAccess(compute_read),
        &.{},
    );

    const draw_commands = try builder.createBuffer(
        pass_handle,
        "Debug Indirect draw command buffer",
        gpu_buffer.defaultBufferProperties(
            debug_geometry_count_max,
            @sizeOf(vk.DrawIndexedIndirectCommand),
            .{ .indirect_buffer = true, .storage_buffer = true },
        ),
        fg.toBufferAccess(compute_write),
        &.{},
    );

    const instance_buffer = try builder.createBuffer(
        pass_handle,
        "Debug geometry instance buffer",
        gpu_buffer.defaultBufferProperties(
            debug_geometry_count_max,
            @sizeOf(hlsl_private.DebugGeometryInstance),
            .{ .storage_buffer = true },
        ),
        fg.toBufferAccess(compute_write),
        &.{},
    );

    return .{
        .pass_handle = pass_handle,
        .draw_counter = draw_counter,
        .user_commands_buffer = user_commands_buffer,
        .draw_commands = draw_commands,
        .instance_buffer = instance_buffer,
    };
}

pub fn createDrawFrameGraphRecord(
    builder: *Builder,
    build_cmds: ComputeFrameGraphRecord,
    draw_counter_handle: fg.ResourceUsageHandle,
    scene_hdr_usage_handle: fg.ResourceUsageHandle,
    scene_depth_usage_handle: fg.ResourceUsageHandle,
) !DrawFrameGraphRecord {
    const pass_handle = try builder.createRenderPass("Debug Geometry Draw", false);

    const scene_hdr = try builder.writeTexture(
        pass_handle,
        scene_hdr_usage_handle,
        fg.toTextureAccess(.{
            .stage_mask = .{ .color_attachment_output_bit = true },
            .access_mask = .{ .color_attachment_write_bit = true },
            .image_layout = .color_attachment_optimal,
        }),
        &.{},
    );

    const scene_depth = try builder.readTexture(
        pass_handle,
        scene_depth_usage_handle,
        fg.toTextureAccess(.{
            .stage_mask = .{ .early_fragment_tests_bit = true, .late_fragment_tests_bit = true },
            .access_mask = .{ .depth_stencil_attachment_read_bit = true },
            .image_layout = .depth_read_only_optimal,
        }),
        &.{},
    );

    const draw_counter = try builder.readBuffer(pass_handle, draw_counter_handle, fg.toBufferAccess(indirect_read), &.{});
    const draw_commands = try builder.readBuffer(pass_handle, build_cmds.draw_commands, fg.toBufferAccess(indirect_read), &.{});

    const instance_buffer = try builder.readBuffer(
        pass_handle,
        build_cmds.instance_buffer,
        fg.toBufferAccess(.{
            .stage_mask = .{ .vertex_shader_bit = true },
            .access_mask = .{ .shader_read_bit = true },
            .image_layout = .undefined,
        }),
        &.{},
    );

    return .{
        .pass_handle = pass_handle,
        .scene_hdr = scene_hdr,
        .scene_depth = scene_depth,
        .draw_counter = draw_counter,
        .draw_commands = draw_commands,
        .instance_buffer = instance_buffer,
    };
}

// --------------------------------------------------------------------------
// Descriptor updates
// --------------------------------------------------------------------------

pub fn updateStartResources(
    vma_instance: vma.VmaAllocator,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const DebugGeometryPassResources,
) !void {
    const commands = prepared.debug_draw_commands.items;
    if (commands.len == 0) return;

    std.debug.assert(commands.len <= max_cpu_debug_command_count);

    try buffer_module.uploadBufferData(
        vma_instance,
        resources.cpu_commands_staging_buffer,
        resources.cpu_commands_staging_properties,
        std.mem.sliceAsBytes(commands),
        0,
    );
}

pub fn updateBuildCmdsResources(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: ComputeFrameGraphRecord,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const DebugGeometryPassResources,
    vma_instance: vma.VmaAllocator,
) !void {
    var pass_constants = hlsl_private.DebugGeometryBuildCmdsPassConstants{
        .main_camera_ws_to_cs = prepared.forward_pass_constants.ws_to_cs_matrix,
    };

    for (resources.proxy_mesh_allocs, 0..) |in_alloc, i| {
        pass_constants.debug_geometry_allocs[i] = .{
            .index_offset = in_alloc.index_offset,
            .index_count = in_alloc.index_count,
            .vertex_offset = in_alloc.vertex_offset,
        };
    }

    try buffer_module.uploadBufferData(
        vma_instance,
        resources.build_cmds_constants,
        resources.build_cmds_constants_properties,
        std.mem.asBytes(&pass_constants),
        0,
    );

    const draw_counter = frame_graph_resources.getBuffer(framegraph, record.draw_counter);
    const user_commands = frame_graph_resources.getBuffer(framegraph, record.user_commands_buffer);
    const draw_commands = frame_graph_resources.getBuffer(framegraph, record.draw_commands);
    const instance_buffer = frame_graph_resources.getBuffer(framegraph, record.instance_buffer);

    const set = resources.build_cmds_descriptor_set;

    write_helper.appendBuffer(set, 0, .uniform_buffer, resources.build_cmds_constants.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, 1, .storage_buffer, draw_counter.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, 2, .storage_buffer, user_commands.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, 3, .storage_buffer, draw_commands.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, 4, .storage_buffer, instance_buffer.handle, 0, vk.WHOLE_SIZE);
}

pub fn updateDrawDescriptorSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: DrawFrameGraphRecord,
    resources: *const DebugGeometryPassResources,
) void {
    const instance_buffer = frame_graph_resources.getBuffer(framegraph, record.instance_buffer);

    const set = resources.draw_descriptor_set;

    write_helper.appendBuffer(set, 0, .storage_buffer, resources.vertex_buffer_position.handle, 0, vk.WHOLE_SIZE);
    write_helper.appendBuffer(set, 1, .storage_buffer, instance_buffer.handle, 0, vk.WHOLE_SIZE);
}

// --------------------------------------------------------------------------
// Recording
// --------------------------------------------------------------------------

pub fn recordStartCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    record: StartFrameGraphRecord,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const DebugGeometryPassResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const cpu_command_count: u32 = @intCast(prepared.debug_draw_commands.items.len);

    const draw_counter = helper.resources.getBuffer(helper.frame_graph, record.draw_counter);

    // The counter is seeded with the CPU command count; the compute pass
    // appends to it for any GPU-side producer.
    vkd.cmdFillBuffer(
        cmd_buffer,
        draw_counter.handle,
        draw_counter.default_view.offset_bytes,
        draw_counter.default_view.size_bytes,
        cpu_command_count,
    );

    if (cpu_command_count == 0) return;

    const user_commands_buffer = helper.resources.getBuffer(helper.frame_graph, record.user_commands_buffer);

    const regions = [_]vk.BufferCopy2{.{
        .s_type = .buffer_copy_2,
        .p_next = null,
        .src_offset = 0,
        .dst_offset = 0,
        .size = cpu_command_count * resources.cpu_commands_staging_properties.element_size_bytes,
    }};

    const copy = vk.CopyBufferInfo2{
        .s_type = .copy_buffer_info_2,
        .p_next = null,
        .src_buffer = resources.cpu_commands_staging_buffer.handle,
        .dst_buffer = user_commands_buffer.handle,
        .region_count = regions.len,
        .p_regions = &regions,
    };

    vkd.cmdCopyBuffer2(cmd_buffer, &copy);
}

pub fn recordBuildCmdsCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: ComputeFrameGraphRecord,
    resources: *const DebugGeometryPassResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(resources.build_cmds.pipeline_index));

    const sets = [_]vk.DescriptorSet{resources.build_cmds_descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.build_cmds.pipeline_layout, 0, &sets, &.{});

    // Since we know the actual debug geometry count on the GPU, we could use
    // an indirect dispatch instead. Doing it this way wastes a bit of
    // resources, but it's negligible to do this ATM for debug builds.
    vkd.cmdDispatch(
        cmd_buffer,
        divRoundUp(debug_geometry_count_max, hlsl_private.DebugGeometryBuildCmdsThreadCount),
        1,
        1,
    );
}

pub fn recordDrawCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: DrawFrameGraphRecord,
    resources: *const DebugGeometryPassResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const hdr_buffer = helper.resources.getTexture(helper.frame_graph, record.scene_hdr);
    const depth_texture = helper.resources.getTexture(helper.frame_graph, record.scene_depth);
    const draw_counter = helper.resources.getBuffer(helper.frame_graph, record.draw_counter);
    const draw_commands = helper.resources.getBuffer(helper.frame_graph, record.draw_commands);

    const depth_extent = vk.Extent2D{
        .width = depth_texture.properties.width,
        .height = depth_texture.properties.height,
    };

    const pass_rect = render_pass_helpers.defaultRect(depth_extent);
    const viewports = [_]vk.Viewport{render_pass_helpers.defaultViewport(pass_rect)};
    const scissors = [_]vk.Rect2D{pass_rect};

    vkd.cmdBindPipeline(cmd_buffer, .graphics, pipeline_factory.getPipeline(resources.draw.pipeline_index));

    vkd.cmdSetViewport(cmd_buffer, 0, &viewports);
    vkd.cmdSetScissor(cmd_buffer, 0, &scissors);

    const color_attachments = [_]vk.RenderingAttachmentInfo{
        pipeline_module.defaultRenderingAttachmentInfo(hdr_buffer.default_view_handle, hdr_buffer.image_layout),
    };

    const depth_attachment = pipeline_module.defaultRenderingAttachmentInfo(
        depth_texture.default_view_handle,
        depth_texture.image_layout,
    );

    const rendering_info = pipeline_module.defaultRenderingInfo(pass_rect, &color_attachments, &depth_attachment);

    vkd.cmdBeginRendering(cmd_buffer, &rendering_info);

    const sets = [_]vk.DescriptorSet{resources.draw_descriptor_set};
    vkd.cmdBindDescriptorSets(cmd_buffer, .graphics, resources.draw.pipeline_layout, 0, &sets, &.{});

    vkd.cmdBindIndexBuffer2(cmd_buffer, resources.index_buffer.handle, 0, vk.WHOLE_SIZE, .uint32);

    vkd.cmdDrawIndexedIndirectCount(
        cmd_buffer,
        draw_commands.handle,
        0,
        draw_counter.handle,
        0,
        debug_geometry_count_max,
        draw_commands.properties.element_size_bytes,
    );

    vkd.cmdEndRendering(cmd_buffer);
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "the indirect command buffer stride matches VkDrawIndexedIndirectCommand" {
    // The compute pass writes raw VkDrawIndexedIndirectCommand structs, and
    // the draw reads them back with the buffer's element size as the stride —
    // a mismatch here would walk the buffer at the wrong pitch.
    try testing.expectEqual(@as(usize, 20), @sizeOf(vk.DrawIndexedIndirectCommand));
}

test "the proxy mesh table is indexed by the shader's geometry type enum" {
    try testing.expectEqual(@as(usize, 2), proxy_mesh_paths.len);
    try testing.expect(std.mem.endsWith(u8, proxy_mesh_paths[hlsl_public.DebugGeometryType_Icosphere], "icosahedron.obj"));
    try testing.expect(std.mem.endsWith(u8, proxy_mesh_paths[hlsl_public.DebugGeometryType_Box], "box.obj"));
}
