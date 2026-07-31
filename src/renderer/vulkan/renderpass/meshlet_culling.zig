// Port of src/renderer/vulkan/renderpass/MeshletCulling.{h,cpp}
//
// Five passes that turn the whole meshlet set into a compact indexed indirect
// draw per culling pass:
//
//   Clear                : zero the counter buffer
//   CullMeshlets         : one dispatch per cull command, cone/frustum reject
//   CullTrianglesPrepare : turn the surviving meshlet count into a dispatch arg
//   CullTriangles        : indirect dispatch, backface/small-triangle reject
//   Debug                : copy the counters back for the CPU-side stats
//
// Every buffer is sub-allocated per culling pass out of one bigger buffer, so a
// lot of the code here is offset arithmetic on `BufferSubresource`.

const std = @import("std");
const vk = @import("vulkan");

const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const gpu_buffer = @import("../../buffer/gpu_buffer.zig");
const pipeline_module = @import("../pipeline.zig");
const shader_modules = @import("../shader_modules.zig");

const hlsl_meshlet = @import("../../hlsl/meshlet/meshlet.zig");
const hlsl_culling = @import("../../hlsl/meshlet/meshlet_culling.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const DescriptorWriteHelper = descriptor_set.DescriptorWriteHelper;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const MeshCache = @import("../mesh_cache.zig").MeshCache;
const PipelineFactory = @import("../pipeline_factory.zig").PipelineFactory;
const StorageBufferAllocator = @import("../storage_buffer.zig").StorageBufferAllocator;
const StorageBufferAlloc = @import("../storage_buffer.zig").StorageBufferAlloc;
const barrier_module = @import("../barrier.zig");
const buffer_module = @import("../buffer.zig");
const divRoundUp = @import("../compute_helper.zig").divRoundUp;
const prepare_buckets = @import("../../prepare_buckets.zig");
const vma = @import("../vma.zig").c;

const log = std.log.scoped(.renderer);

// --------------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------------

const index_size_bytes: u32 = 1;
/// NOTE: Because of u8 indices we pack a triangle in 24 bits + 8 bits for a
/// prim restart
const triangle_indices_size_bytes: u32 = 4;
pub const max_meshlet_culling_pass_count: u32 = 4;
/// NOTE: Increasing this seems to make perf degrade noticeably on my intel iGPU
/// for the same amount of geometry drawn.
const max_visible_meshlets_per_pass: u32 = 4096;

/// Worst case if all meshlets of all passes aren't culled.
/// This shouldn't happen, we can probably cut this by half and raise a warning
/// when we cross the limit.
const visible_index_buffer_size_bytes: u64 = @as(u64, max_visible_meshlets_per_pass) *
    max_meshlet_culling_pass_count * hlsl_meshlet.MeshletMaxTriangleCount * triangle_indices_size_bytes;

const max_indirect_draw_count_per_pass: u32 = max_visible_meshlets_per_pass;

/// sizeof(VkDispatchIndirectCommand)
const dispatch_indirect_command_size: u32 = 3 * @sizeOf(u32);
/// sizeof(VkDrawIndexedIndirectCommand)
const draw_indexed_indirect_command_size: u32 = 5 * @sizeOf(u32);

comptime {
    std.debug.assert(dispatch_indirect_command_size == @sizeOf(vk.DispatchIndirectCommand));
    std.debug.assert(draw_indexed_indirect_command_size == @sizeOf(vk.DrawIndexedIndirectCommand));
}

// --------------------------------------------------------------------------
// Descriptor bindings
// --------------------------------------------------------------------------

fn storageBufferBinding(slot: u32) descriptor_set.DescriptorBinding {
    return .{ .slot = slot, .count = 1, .type = .storage_buffer, .stage_mask = .{ .compute_bit = true } };
}

const CullMeshletsBinding = enum(u32) {
    meshlets = 0,
    cull_mesh_instance_params = 1,
    counters = 2,
    meshlets_offsets_out = 3,

    const count = 4;
};

const CullTrianglesPrepareBinding = enum(u32) {
    counters = 0,
    indirect_dispatch_out = 1,

    const count = 2;
};

const CullTrianglesBinding = enum(u32) {
    meshlets = 0,
    indices = 1,
    buffer_position_ms = 2,
    cull_mesh_instance_params = 3,
    visible_index_buffer = 4,
    draw_command_out = 5,
    counters = 6,
    visible_meshlets = 7,

    const count = 8;
};

fn makeBindings(comptime count: u32) [count]descriptor_set.DescriptorBinding {
    var bindings: [count]descriptor_set.DescriptorBinding = undefined;
    for (&bindings, 0..) |*binding, i| binding.* = storageBufferBinding(@intCast(i));
    return bindings;
}

// --------------------------------------------------------------------------
// Pipelines
// --------------------------------------------------------------------------

/// FIXME (kept from the C++ name)
pub const SimplePipeline = struct {
    pipeline_index: u32,
    pipeline_layout: vk.PipelineLayout,
    descriptor_set_layout: vk.DescriptorSetLayout,

    fn deinit(self: SimplePipeline, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.descriptor_set_layout, null);
    }
};

fn makeComputePipelineCreator(comptime shader_name: []const u8) pipeline_factory_module.PipelineFunctor {
    return &struct {
        fn create(
            vkd: *const vk.DeviceWrapper,
            device: vk.Device,
            pipeline_layout: vk.PipelineLayout,
        ) anyerror!vk.Pipeline {
            const module_create_info = pipeline_module.shaderModuleCreateInfo(shader_modules.get(shader_name));

            return pipeline_module.createComputePipeline(
                vkd,
                device,
                pipeline_layout,
                pipeline_module.defaultPipelineShaderStageCreateInfo(
                    .{ .compute_bit = true },
                    &module_create_info,
                    null,
                ),
            );
        }
    }.create;
}

const pipeline_factory_module = @import("../pipeline_factory.zig");

// --------------------------------------------------------------------------
// Resources
// --------------------------------------------------------------------------

pub const MeshletCullingResources = struct {
    cull_meshlets_pipe: SimplePipeline,
    cull_meshlets_prep_indirect_pipe: SimplePipeline,
    cull_triangles_pipe: SimplePipeline,

    cull_meshlet_descriptor_sets: [max_meshlet_culling_pass_count]vk.DescriptorSet,
    cull_triangles_descriptor_sets: [max_meshlet_culling_pass_count]vk.DescriptorSet,
    cull_prepare_descriptor_set: vk.DescriptorSet,

    counters_cpu_buffer: buffer_module.GPUBuffer,
    counters_cpu_properties: gpu_buffer.GPUBufferProperties,

    counters_ready_event: vk.Event,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
        vma_instance: vma.VmaAllocator,
        pipeline_factory: *PipelineFactory,
        max_draw_indirect_count: u32,
    ) !MeshletCullingResources {
        const cull_meshlets_pipe = try createSimplePipeline(
            vkd,
            device,
            pipeline_factory,
            &makeBindings(CullMeshletsBinding.count),
            @sizeOf(hlsl_culling.CullMeshletPushConstants),
            makeComputePipelineCreator("meshlet/cull_meshlet.comp.spv"),
        );
        errdefer cull_meshlets_pipe.deinit(vkd, device);

        // NOTE: the C++ sizes this push constant range with
        // CullMeshletPushConstants even though the shader takes none. Kept.
        const cull_meshlets_prep_indirect_pipe = try createSimplePipeline(
            vkd,
            device,
            pipeline_factory,
            &makeBindings(CullTrianglesPrepareBinding.count),
            @sizeOf(hlsl_culling.CullMeshletPushConstants),
            makeComputePipelineCreator("meshlet/prepare_fine_culling_indirect.comp.spv"),
        );
        errdefer cull_meshlets_prep_indirect_pipe.deinit(vkd, device);

        const cull_triangles_pipe = try createSimplePipeline(
            vkd,
            device,
            pipeline_factory,
            &makeBindings(CullTrianglesBinding.count),
            @sizeOf(hlsl_culling.CullPushConstants),
            makeComputePipelineCreator("meshlet/cull_triangle_batch.comp.spv"),
        );
        errdefer cull_triangles_pipe.deinit(vkd, device);

        const counters_cpu_properties = gpu_buffer.defaultBufferProperties(
            hlsl_culling.CountersCount * max_meshlet_culling_pass_count,
            @sizeOf(u32),
            .{ .transfer_dst = true },
        );

        const counters_cpu_buffer = try buffer_module.createBuffer(
            vma_instance,
            counters_cpu_properties,
            .cpu_only,
        );
        errdefer buffer_module.destroyBuffer(vma_instance, counters_cpu_buffer);

        std.debug.assert(max_indirect_draw_count_per_pass < max_draw_indirect_count);

        var cull_meshlet_descriptor_sets: [max_meshlet_culling_pass_count]vk.DescriptorSet = undefined;
        var cull_triangles_descriptor_sets: [max_meshlet_culling_pass_count]vk.DescriptorSet = undefined;
        var cull_prepare_descriptor_set: [1]vk.DescriptorSet = undefined;

        try allocateSetsWithLayout(
            vkd,
            device,
            descriptor_pool,
            cull_meshlets_pipe.descriptor_set_layout,
            &cull_meshlet_descriptor_sets,
        );
        try pipeline_module.allocateDescriptorSets(
            vkd,
            device,
            descriptor_pool,
            &.{cull_meshlets_prep_indirect_pipe.descriptor_set_layout},
            &cull_prepare_descriptor_set,
        );
        try allocateSetsWithLayout(
            vkd,
            device,
            descriptor_pool,
            cull_triangles_pipe.descriptor_set_layout,
            &cull_triangles_descriptor_sets,
        );

        // NOTE: not DEVICE_ONLY, unlike the frame graph's events — this one is
        // waited on by the CPU-side stats readback.
        const counters_ready_event = try vkd.createEvent(device, &.{
            .s_type = .event_create_info,
            .p_next = null,
            .flags = .{},
        }, null);

        return .{
            .cull_meshlets_pipe = cull_meshlets_pipe,
            .cull_meshlets_prep_indirect_pipe = cull_meshlets_prep_indirect_pipe,
            .cull_triangles_pipe = cull_triangles_pipe,
            .cull_meshlet_descriptor_sets = cull_meshlet_descriptor_sets,
            .cull_triangles_descriptor_sets = cull_triangles_descriptor_sets,
            .cull_prepare_descriptor_set = cull_prepare_descriptor_set[0],
            .counters_cpu_buffer = counters_cpu_buffer,
            .counters_cpu_properties = counters_cpu_properties,
            .counters_ready_event = counters_ready_event,
        };
    }

    pub fn deinit(self: *MeshletCullingResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        buffer_module.destroyBuffer(vma_instance, self.counters_cpu_buffer);

        self.cull_meshlets_pipe.deinit(vkd, device);
        self.cull_meshlets_prep_indirect_pipe.deinit(vkd, device);
        self.cull_triangles_pipe.deinit(vkd, device);

        vkd.destroyEvent(device, self.counters_ready_event, null);
    }
};

fn createSimplePipeline(
    vkd: anytype,
    device: vk.Device,
    pipeline_factory: *PipelineFactory,
    bindings: []const descriptor_set.DescriptorBinding,
    push_constant_size: u32,
    creator: pipeline_factory_module.PipelineFunctor,
) !SimplePipeline {
    var layout_bindings_buffer: [16]vk.DescriptorSetLayoutBinding = undefined;
    const layout_bindings = layout_bindings_buffer[0..bindings.len];
    descriptor_set.fillLayoutBindings(layout_bindings, bindings);

    const descriptor_set_layout = try pipeline_module.createDescriptorSetLayout(vkd, device, layout_bindings, &.{});
    errdefer vkd.destroyDescriptorSetLayout(device, descriptor_set_layout, null);

    const push_constant_ranges = [_]vk.PushConstantRange{.{
        .stage_flags = .{ .compute_bit = true },
        .offset = 0,
        .size = push_constant_size,
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
        .pipeline_creation_function = creator,
    });

    return .{
        .pipeline_index = pipeline_index,
        .pipeline_layout = pipeline_layout,
        .descriptor_set_layout = descriptor_set_layout,
    };
}

fn allocateSetsWithLayout(
    vkd: anytype,
    device: vk.Device,
    descriptor_pool: vk.DescriptorPool,
    layout: vk.DescriptorSetLayout,
    out: []vk.DescriptorSet,
) !void {
    var layouts: [max_meshlet_culling_pass_count]vk.DescriptorSetLayout = undefined;
    @memset(layouts[0..out.len], layout);

    try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, layouts[0..out.len], out);
}

// --------------------------------------------------------------------------
// Frame graph record
// --------------------------------------------------------------------------

pub const CullMeshletsFrameGraphRecord = struct {
    pub const Clear = struct {
        pass_handle: fg.RenderPassHandle,
        meshlet_counters: fg.ResourceUsageHandle,
    };

    pub const CullMeshlets = struct {
        pass_handle: fg.RenderPassHandle,
        meshlet_counters: fg.ResourceUsageHandle,
        visible_meshlet_offsets: fg.ResourceUsageHandle,
    };

    pub const CullTrianglesPrepare = struct {
        pass_handle: fg.RenderPassHandle,
        meshlet_counters: fg.ResourceUsageHandle,
        indirect_dispatch_buffer: fg.ResourceUsageHandle,
    };

    pub const CullTriangles = struct {
        pass_handle: fg.RenderPassHandle,
        indirect_dispatch_buffer: fg.ResourceUsageHandle,
        meshlet_counters: fg.ResourceUsageHandle,
        visible_meshlet_offsets: fg.ResourceUsageHandle,
        meshlet_indirect_draw_commands: fg.ResourceUsageHandle,
        meshlet_visible_index_buffer: fg.ResourceUsageHandle,
        visible_meshlet_buffer: fg.ResourceUsageHandle,
    };

    pub const Debug = struct {
        pass_handle: fg.RenderPassHandle,
        meshlet_counters: fg.ResourceUsageHandle,
    };

    clear: Clear,
    cull_meshlets: CullMeshlets,
    cull_triangles_prepare: CullTrianglesPrepare,
    cull_triangles: CullTriangles,
    debug: Debug,
};

pub fn createFrameGraphRecord(builder: *Builder) !CullMeshletsFrameGraphRecord {
    // ---- Clear ----
    const clear_pass = try builder.createRenderPass("Meshlet Culling Clear", false);

    const clear_meshlet_counters = try builder.createBuffer(
        clear_pass,
        "Meshlet counters",
        gpu_buffer.defaultBufferProperties(
            hlsl_culling.CountersCount * max_meshlet_culling_pass_count,
            @sizeOf(u32),
            .{ .indirect_buffer = true, .transfer_src = true, .transfer_dst = true, .storage_buffer = true },
        ),
        .{ .stage_mask = .{ .all_transfer_bit = true }, .access_mask = .{ .transfer_write_bit = true } },
        &.{},
    );

    // ---- Cull meshlets ----
    const cull_meshlets_pass = try builder.createRenderPass("Cull Meshlets", false);

    const cull_meshlets_counters = try builder.writeBuffer(
        cull_meshlets_pass,
        clear_meshlet_counters,
        .{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_storage_write_bit = true, .shader_storage_read_bit = true },
        },
        &.{},
    );

    const visible_meshlet_offsets = try builder.createBuffer(
        cull_meshlets_pass,
        "Visible meshlet offsets buffer",
        gpu_buffer.defaultBufferProperties(
            max_visible_meshlets_per_pass * max_meshlet_culling_pass_count,
            @sizeOf(hlsl_culling.MeshletOffsets),
            .{ .storage_buffer = true },
        ),
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_write_bit = true } },
        &.{},
    );

    // ---- Cull triangles prepare ----
    const prepare_pass = try builder.createRenderPass("Cull Triangles Prepare", false);

    const prepare_counters = try builder.readBuffer(
        prepare_pass,
        cull_meshlets_counters,
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
        &.{},
    );

    const prepare_indirect_dispatch = try builder.createBuffer(
        prepare_pass,
        "Meshlet indirect dispatch buffer",
        gpu_buffer.defaultBufferProperties(
            max_meshlet_culling_pass_count,
            dispatch_indirect_command_size,
            .{ .indirect_buffer = true, .storage_buffer = true },
        ),
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_write_bit = true } },
        &.{},
    );

    // ---- Cull triangles ----
    const cull_triangles_pass = try builder.createRenderPass("Cull Triangles", false);

    const cull_triangles_indirect_dispatch = try builder.readBuffer(
        cull_triangles_pass,
        prepare_indirect_dispatch,
        .{
            .stage_mask = .{ .draw_indirect_bit = true },
            .access_mask = .{ .indirect_command_read_bit = true },
        },
        &.{},
    );

    const cull_triangles_counters = try builder.writeBuffer(
        cull_triangles_pass,
        cull_meshlets_counters,
        .{
            .stage_mask = .{ .compute_shader_bit = true },
            .access_mask = .{ .shader_write_bit = true, .shader_read_bit = true },
        },
        &.{},
    );

    const cull_triangles_visible_offsets = try builder.readBuffer(
        cull_triangles_pass,
        visible_meshlet_offsets,
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_read_bit = true } },
        &.{},
    );

    const meshlet_indirect_draw_commands = try builder.createBuffer(
        cull_triangles_pass,
        "Meshlet Indirect draw commands buffer",
        gpu_buffer.defaultBufferProperties(
            max_indirect_draw_count_per_pass * max_meshlet_culling_pass_count,
            draw_indexed_indirect_command_size,
            .{ .indirect_buffer = true, .storage_buffer = true },
        ),
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_write_bit = true } },
        &.{},
    );

    // NOTE: despite the name, visible_index_buffer_size_bytes is the size of
    // ONE pass's slice — it folds in the pass count because a single pass is
    // provisioned for the worst case where no meshlet of any pass is culled.
    // Multiplying by the pass count again is therefore correct: the four
    // slices tile this buffer exactly.
    const meshlet_visible_index_buffer = try builder.createBuffer(
        cull_triangles_pass,
        "Meshlet visible index buffer",
        gpu_buffer.defaultBufferProperties(
            visible_index_buffer_size_bytes * max_meshlet_culling_pass_count,
            1,
            .{ .index_buffer = true, .storage_buffer = true },
        ),
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_write_bit = true } },
        &.{},
    );

    // NOTE: This buffer should only be used in the main pass.
    const visible_meshlet_buffer = try builder.createBuffer(
        cull_triangles_pass,
        "Visible meshlet buffer",
        gpu_buffer.defaultBufferProperties(
            max_indirect_draw_count_per_pass,
            @sizeOf(hlsl_meshlet.VisibleMeshlet),
            .{ .storage_buffer = true },
        ),
        .{ .stage_mask = .{ .compute_shader_bit = true }, .access_mask = .{ .shader_write_bit = true } },
        &.{},
    );

    // ---- Debug ----
    const debug_pass = try builder.createRenderPass("Debug", true);

    const debug_counters = try builder.readBuffer(
        debug_pass,
        cull_triangles_counters,
        .{ .stage_mask = .{ .all_transfer_bit = true }, .access_mask = .{ .transfer_read_bit = true } },
        &.{},
    );

    return .{
        .clear = .{ .pass_handle = clear_pass, .meshlet_counters = clear_meshlet_counters },
        .cull_meshlets = .{
            .pass_handle = cull_meshlets_pass,
            .meshlet_counters = cull_meshlets_counters,
            .visible_meshlet_offsets = visible_meshlet_offsets,
        },
        .cull_triangles_prepare = .{
            .pass_handle = prepare_pass,
            .meshlet_counters = prepare_counters,
            .indirect_dispatch_buffer = prepare_indirect_dispatch,
        },
        .cull_triangles = .{
            .pass_handle = cull_triangles_pass,
            .indirect_dispatch_buffer = cull_triangles_indirect_dispatch,
            .meshlet_counters = cull_triangles_counters,
            .visible_meshlet_offsets = cull_triangles_visible_offsets,
            .meshlet_indirect_draw_commands = meshlet_indirect_draw_commands,
            .meshlet_visible_index_buffer = meshlet_visible_index_buffer,
            .visible_meshlet_buffer = visible_meshlet_buffer,
        },
        .debug = .{ .pass_handle = debug_pass, .meshlet_counters = debug_counters },
    };
}

// --------------------------------------------------------------------------
// Draw parameters
// --------------------------------------------------------------------------

pub const MeshletDrawParams = struct {
    counter_buffer_offset: u64,
    index_buffer_offset: u64,
    index_type: vk.IndexType,
    command_buffer_offset: u64,
    command_buffer_max_count: u32,
};

fn meshletIndexType() vk.IndexType {
    return switch (index_size_bytes) {
        1 => .uint8,
        2 => .uint16,
        4 => .uint32,
        else => unreachable,
    };
}

pub fn getMeshletDrawParams(pass_index: u32) MeshletDrawParams {
    return .{
        .counter_buffer_offset = (@as(u64, pass_index) * hlsl_culling.CountersCount +
            hlsl_culling.DrawCommandCounterOffset) * @sizeOf(u32),
        .index_buffer_offset = @as(u64, pass_index) * visible_index_buffer_size_bytes,
        .index_type = meshletIndexType(),
        .command_buffer_offset = @as(u64, pass_index) * max_indirect_draw_count_per_pass *
            draw_indexed_indirect_command_size,
        .command_buffer_max_count = max_indirect_draw_count_per_pass,
    };
}

pub fn getMeshletVisibleIndexBufferPass(pass_index: u32) gpu_buffer.BufferSubresource {
    return .{
        .element_offset = @as(u64, pass_index) * visible_index_buffer_size_bytes,
        .element_count = visible_index_buffer_size_bytes,
    };
}

// --------------------------------------------------------------------------
// Descriptor updates
// --------------------------------------------------------------------------

pub fn updatePassesResources(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: CullMeshletsFrameGraphRecord,
    frame_storage_allocator: *StorageBufferAllocator,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const MeshletCullingResources,
    mesh_cache: *const MeshCache,
) void {
    if (prepared.cull_mesh_instance_params.items.len == 0) return;

    const mesh_instance_alloc = frame_storage_allocator.allocateAndUpload(
        hlsl_culling.CullMeshInstanceParams,
        prepared.cull_mesh_instance_params.items,
    );

    updateCullMeshletsSets(
        write_helper,
        framegraph,
        frame_graph_resources,
        record.cull_meshlets,
        prepared,
        resources,
        mesh_cache,
        mesh_instance_alloc,
    );

    updateCullTrianglesPrepareSets(
        write_helper,
        framegraph,
        frame_graph_resources,
        record.cull_triangles_prepare,
        resources,
    );

    updateCullTrianglesSets(
        write_helper,
        framegraph,
        frame_graph_resources,
        record.cull_triangles,
        prepared,
        resources,
        mesh_cache,
        mesh_instance_alloc,
    );
}

fn appendWholeBuffer(
    write_helper: *DescriptorWriteHelper,
    set: vk.DescriptorSet,
    binding: anytype,
    handle: vk.Buffer,
) void {
    write_helper.appendBuffer(set, @intFromEnum(binding), .storage_buffer, handle, 0, vk.WHOLE_SIZE);
}

fn appendBufferView(
    write_helper: *DescriptorWriteHelper,
    set: vk.DescriptorSet,
    binding: anytype,
    handle: vk.Buffer,
    view: gpu_buffer.GPUBufferView,
) void {
    write_helper.appendBuffer(set, @intFromEnum(binding), .storage_buffer, handle, view.offset_bytes, view.size_bytes);
}

fn updateCullMeshletsSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: CullMeshletsFrameGraphRecord.CullMeshlets,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const MeshletCullingResources,
    mesh_cache: *const MeshCache,
    mesh_instance_alloc: StorageBufferAlloc,
) void {
    const meshlet_counters = frame_graph_resources.getBuffer(framegraph, record.meshlet_counters);
    const visible_meshlet_offsets = frame_graph_resources.getBuffer(framegraph, record.visible_meshlet_offsets);

    for (prepared.cull_passes.items) |cull_pass| {
        const pass_index = cull_pass.pass_index;
        std.debug.assert(pass_index < max_meshlet_culling_pass_count);

        const counter_view = gpu_buffer.getBufferView(meshlet_counters.properties, .{
            .element_offset = pass_index * hlsl_culling.CountersCount,
            .element_count = hlsl_culling.CountersCount,
        });

        const offsets_view = gpu_buffer.getBufferView(visible_meshlet_offsets.properties, .{
            .element_offset = pass_index * max_visible_meshlets_per_pass,
            .element_count = max_visible_meshlets_per_pass,
        });

        const set = resources.cull_meshlet_descriptor_sets[pass_index];

        appendWholeBuffer(write_helper, set, CullMeshletsBinding.meshlets, mesh_cache.meshlet_buffer.handle);
        write_helper.appendBuffer(
            set,
            @intFromEnum(CullMeshletsBinding.cull_mesh_instance_params),
            .storage_buffer,
            mesh_instance_alloc.buffer,
            mesh_instance_alloc.offset_bytes,
            mesh_instance_alloc.size_bytes,
        );
        appendBufferView(write_helper, set, CullMeshletsBinding.counters, meshlet_counters.handle, counter_view);
        appendBufferView(
            write_helper,
            set,
            CullMeshletsBinding.meshlets_offsets_out,
            visible_meshlet_offsets.handle,
            offsets_view,
        );
    }
}

fn updateCullTrianglesPrepareSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: CullMeshletsFrameGraphRecord.CullTrianglesPrepare,
    resources: *const MeshletCullingResources,
) void {
    const meshlet_counters = frame_graph_resources.getBuffer(framegraph, record.meshlet_counters);
    const indirect_dispatch = frame_graph_resources.getBuffer(framegraph, record.indirect_dispatch_buffer);

    const set = resources.cull_prepare_descriptor_set;

    appendWholeBuffer(write_helper, set, CullTrianglesPrepareBinding.counters, meshlet_counters.handle);
    appendWholeBuffer(write_helper, set, CullTrianglesPrepareBinding.indirect_dispatch_out, indirect_dispatch.handle);
}

fn updateCullTrianglesSets(
    write_helper: *DescriptorWriteHelper,
    framegraph: *const fg.FrameGraph,
    frame_graph_resources: *const FrameGraphResources,
    record: CullMeshletsFrameGraphRecord.CullTriangles,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const MeshletCullingResources,
    mesh_cache: *const MeshCache,
    mesh_instance_alloc: StorageBufferAlloc,
) void {
    const meshlet_counters = frame_graph_resources.getBuffer(framegraph, record.meshlet_counters);
    const visible_meshlet_offsets = frame_graph_resources.getBuffer(framegraph, record.visible_meshlet_offsets);
    const indirect_draw_commands = frame_graph_resources.getBuffer(framegraph, record.meshlet_indirect_draw_commands);
    const visible_index_buffer = frame_graph_resources.getBuffer(framegraph, record.meshlet_visible_index_buffer);
    const visible_meshlet_buffer = frame_graph_resources.getBuffer(framegraph, record.visible_meshlet_buffer);

    for (prepared.cull_passes.items) |cull_pass| {
        const pass_index = cull_pass.pass_index;
        std.debug.assert(pass_index < max_meshlet_culling_pass_count);

        const counter_view = gpu_buffer.getBufferView(meshlet_counters.properties, .{
            .element_offset = pass_index * hlsl_culling.CountersCount,
            .element_count = hlsl_culling.CountersCount,
        });

        const offsets_view = gpu_buffer.getBufferView(visible_meshlet_offsets.properties, .{
            .element_offset = pass_index * max_visible_meshlets_per_pass,
            .element_count = max_visible_meshlets_per_pass,
        });

        const indices_view = gpu_buffer.getBufferView(
            visible_index_buffer.properties,
            getMeshletVisibleIndexBufferPass(pass_index),
        );

        const draw_view = gpu_buffer.getBufferView(indirect_draw_commands.properties, .{
            .element_offset = pass_index * max_indirect_draw_count_per_pass,
            .element_count = max_indirect_draw_count_per_pass,
        });

        const set = resources.cull_triangles_descriptor_sets[pass_index];

        appendBufferView(write_helper, set, CullTrianglesBinding.meshlets, visible_meshlet_offsets.handle, offsets_view);
        appendWholeBuffer(write_helper, set, CullTrianglesBinding.indices, mesh_cache.index_buffer.handle);
        appendWholeBuffer(
            write_helper,
            set,
            CullTrianglesBinding.buffer_position_ms,
            mesh_cache.vertex_buffer_position.handle,
        );
        write_helper.appendBuffer(
            set,
            @intFromEnum(CullTrianglesBinding.cull_mesh_instance_params),
            .storage_buffer,
            mesh_instance_alloc.buffer,
            mesh_instance_alloc.offset_bytes,
            mesh_instance_alloc.size_bytes,
        );
        appendBufferView(
            write_helper,
            set,
            CullTrianglesBinding.visible_index_buffer,
            visible_index_buffer.handle,
            indices_view,
        );
        appendBufferView(
            write_helper,
            set,
            CullTrianglesBinding.draw_command_out,
            indirect_draw_commands.handle,
            draw_view,
        );
        appendBufferView(write_helper, set, CullTrianglesBinding.counters, meshlet_counters.handle, counter_view);
        appendWholeBuffer(
            write_helper,
            set,
            CullTrianglesBinding.visible_meshlets,
            visible_meshlet_buffer.handle,
        );
    }
}

// --------------------------------------------------------------------------
// Recording
// --------------------------------------------------------------------------

pub fn recordClearCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    record: CullMeshletsFrameGraphRecord.Clear,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const meshlet_counters = helper.resources.getBuffer(helper.frame_graph, record.meshlet_counters);

    vkd.cmdFillBuffer(cmd_buffer, meshlet_counters.handle, 0, vk.WHOLE_SIZE, 0);
}

pub fn recordCullMeshletsCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: CullMeshletsFrameGraphRecord.CullMeshlets,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const MeshletCullingResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    vkd.cmdBindPipeline(cmd_buffer, .compute, pipeline_factory.getPipeline(resources.cull_meshlets_pipe.pipeline_index));

    var total_meshlet_count: u64 = 0;

    for (prepared.cull_passes.items) |cull_pass| {
        var pass_meshlet_count: u64 = 0;

        const sets = [_]vk.DescriptorSet{resources.cull_meshlet_descriptor_sets[cull_pass.pass_index]};
        vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.cull_meshlets_pipe.pipeline_layout, 0, &sets, &.{});

        for (cull_pass.cull_commands.items) |command| {
            vkd.cmdPushConstants(
                cmd_buffer,
                resources.cull_meshlets_pipe.pipeline_layout,
                .{ .compute_bit = true },
                0,
                @sizeOf(hlsl_culling.CullMeshletPushConstants),
                &command.push_constants,
            );

            const group_count_x = divRoundUp(command.push_constants.meshlet_count, hlsl_culling.MeshletCullThreadCount);
            vkd.cmdDispatch(cmd_buffer, group_count_x, command.instance_count, 1);

            pass_meshlet_count += command.push_constants.meshlet_count;
        }

        total_meshlet_count += pass_meshlet_count;

        log.debug("- pass total submitted meshlets = {}, approx. triangles = {}", .{
            pass_meshlet_count,
            pass_meshlet_count * hlsl_meshlet.MeshletMaxTriangleCount,
        });
    }

    log.debug("- total submitted meshlets = {}, approx. triangles = {}", .{
        total_meshlet_count,
        total_meshlet_count * hlsl_meshlet.MeshletMaxTriangleCount,
    });
}

pub fn recordCullTrianglesPrepareCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: CullMeshletsFrameGraphRecord.CullTrianglesPrepare,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const MeshletCullingResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    vkd.cmdBindPipeline(
        cmd_buffer,
        .compute,
        pipeline_factory.getPipeline(resources.cull_meshlets_prep_indirect_pipe.pipeline_index),
    );

    const sets = [_]vk.DescriptorSet{resources.cull_prepare_descriptor_set};
    vkd.cmdBindDescriptorSets(
        cmd_buffer,
        .compute,
        resources.cull_meshlets_prep_indirect_pipe.pipeline_layout,
        0,
        &sets,
        &.{},
    );

    const group_count_x = divRoundUp(
        @intCast(prepared.cull_passes.items.len),
        hlsl_culling.PrepareIndirectDispatchThreadCount,
    );
    vkd.cmdDispatch(cmd_buffer, group_count_x, 1, 1);
}

pub fn recordCullTrianglesCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    pipeline_factory: *const PipelineFactory,
    record: CullMeshletsFrameGraphRecord.CullTriangles,
    prepared: *const prepare_buckets.PreparedData,
    resources: *const MeshletCullingResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const indirect_dispatch_buffer = helper.resources.getBuffer(helper.frame_graph, record.indirect_dispatch_buffer);

    vkd.cmdBindPipeline(
        cmd_buffer,
        .compute,
        pipeline_factory.getPipeline(resources.cull_triangles_pipe.pipeline_index),
    );

    for (prepared.cull_passes.items) |cull_pass| {
        const sets = [_]vk.DescriptorSet{resources.cull_triangles_descriptor_sets[cull_pass.pass_index]};
        vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.cull_triangles_pipe.pipeline_layout, 0, &sets, &.{});

        const consts = hlsl_culling.CullPushConstants{
            .output_size_ts = .{ .x = cull_pass.output_size_ts[0], .y = cull_pass.output_size_ts[1] },
            .main_pass = if (cull_pass.main_pass) 1 else 0,
        };

        vkd.cmdPushConstants(
            cmd_buffer,
            resources.cull_triangles_pipe.pipeline_layout,
            .{ .compute_bit = true },
            0,
            @sizeOf(hlsl_culling.CullPushConstants),
            &consts,
        );

        vkd.cmdDispatchIndirect(
            cmd_buffer,
            indirect_dispatch_buffer.handle,
            @as(u64, cull_pass.pass_index) * dispatch_indirect_command_size,
        );
    }
}

pub fn recordDebugCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    record: CullMeshletsFrameGraphRecord.Debug,
    resources: *const MeshletCullingResources,
) void {
    frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);
    defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.pass_handle);

    const meshlet_counters = helper.resources.getBuffer(helper.frame_graph, record.meshlet_counters);

    const regions = [_]vk.BufferCopy2{.{
        .s_type = .buffer_copy_2,
        .p_next = null,
        .src_offset = 0,
        .dst_offset = 0,
        .size = meshlet_counters.properties.element_count * meshlet_counters.properties.element_size_bytes,
    }};

    const copy = vk.CopyBufferInfo2{
        .s_type = .copy_buffer_info_2,
        .p_next = null,
        .src_buffer = meshlet_counters.handle,
        .dst_buffer = resources.counters_cpu_buffer.handle,
        .region_count = regions.len,
        .p_regions = &regions,
    };

    vkd.cmdCopyBuffer2(cmd_buffer, &copy);

    const buffer_barriers = [_]vk.BufferMemoryBarrier2{barrier_module.getBufferBarrierSameQueue(
        resources.counters_cpu_buffer.handle,
        gpu_buffer.defaultBufferView(resources.counters_cpu_properties),
        .{ .stage_mask = .{ .all_transfer_bit = true }, .access_mask = .{ .transfer_write_bit = true } },
        .{ .stage_mask = .{ .all_commands_bit = true }, .access_mask = .{} },
    )};

    const dependencies = barrier_module.getDependencyInfo(&.{}, &buffer_barriers);

    vkd.cmdSetEvent2(cmd_buffer, resources.counters_ready_event, &dependencies);
    vkd.cmdResetEvent2(cmd_buffer, resources.counters_ready_event, .{ .all_commands_bit = true });
}

// --------------------------------------------------------------------------
// Stats readback
// --------------------------------------------------------------------------

pub const MeshletCullingStats = struct {
    pass_index: u32,
    surviving_meshlet_count: u32,
    surviving_triangle_count: u32,
    indirect_draw_command_count: u32,
};

/// Reads the counters the debug pass copied back. `out` must have room for one
/// entry per culling pass.
pub fn getGpuStats(
    vkd: anytype,
    device: vk.Device,
    vma_instance: vma.VmaAllocator,
    resources: *const MeshletCullingResources,
    non_coherent_atom_size: u64,
    pass_count: usize,
    out: []MeshletCullingStats,
) ![]MeshletCullingStats {
    std.debug.assert(out.len >= pass_count);

    var allocation_info: vma.VmaAllocationInfo = undefined;
    vma.vmaGetAllocationInfo(vma_instance, resources.counters_cpu_buffer.allocation, &allocation_info);

    const mapped_size = std.mem.alignForward(u64, allocation_info.size, non_coherent_atom_size);
    const memory: vk.DeviceMemory = @enumFromInt(@intFromPtr(allocation_info.deviceMemory));

    const mapped = try vkd.mapMemory(device, memory, allocation_info.offset, mapped_size, .{});
    defer vkd.unmapMemory(device, memory);

    const ranges = [_]vk.MappedMemoryRange{.{
        .s_type = .mapped_memory_range,
        .p_next = null,
        .memory = memory,
        .offset = allocation_info.offset,
        .size = mapped_size,
    }};
    vkd.invalidateMappedMemoryRanges(device, &ranges) catch {};

    const counters: [*]const u32 = @ptrCast(@alignCast(mapped.?));

    for (0..pass_count) |i| {
        const base = i * hlsl_culling.CountersCount;

        out[i] = .{
            .pass_index = @intCast(i),
            .surviving_meshlet_count = counters[base + hlsl_culling.MeshletCounterOffset],
            .surviving_triangle_count = counters[base + hlsl_culling.TriangleCounterOffset],
            .indirect_draw_command_count = counters[base + hlsl_culling.DrawCommandCounterOffset],
        };
    }

    return out[0..pass_count];
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "meshlet draw params partition the buffers by pass" {
    var previous_index_end: u64 = 0;
    var previous_command_end: u64 = 0;

    for (0..max_meshlet_culling_pass_count) |i| {
        const params = getMeshletDrawParams(@intCast(i));

        try testing.expectEqual(previous_index_end, params.index_buffer_offset);
        try testing.expectEqual(previous_command_end, params.command_buffer_offset);

        // u8 indices, matching the index_size_bytes constant.
        try testing.expectEqual(vk.IndexType.uint8, params.index_type);
        try testing.expectEqual(max_indirect_draw_count_per_pass, params.command_buffer_max_count);

        previous_index_end += visible_index_buffer_size_bytes;
        previous_command_end += @as(u64, max_indirect_draw_count_per_pass) * draw_indexed_indirect_command_size;
    }
}

test "counter offsets stay inside each pass's slice" {
    for (0..max_meshlet_culling_pass_count) |i| {
        const params = getMeshletDrawParams(@intCast(i));

        const pass_base = @as(u64, @intCast(i)) * hlsl_culling.CountersCount * @sizeOf(u32);
        const pass_end = pass_base + hlsl_culling.CountersCount * @sizeOf(u32);

        try testing.expect(params.counter_buffer_offset >= pass_base);
        try testing.expect(params.counter_buffer_offset < pass_end);
    }
}

test "the per-pass visible index slices tile the buffer exactly" {
    // The name visible_index_buffer_size_bytes is misleading: it already folds
    // in the pass count, but that is because ONE pass is provisioned for the
    // worst case where nothing anywhere is culled. So the buffer is that size
    // times the pass count, and the slices tile it with nothing left over.
    const buffer_size = visible_index_buffer_size_bytes * max_meshlet_culling_pass_count;

    var end: u64 = 0;
    for (0..max_meshlet_culling_pass_count) |i| {
        const subresource = getMeshletVisibleIndexBufferPass(@intCast(i));

        // Contiguous, no gaps, no overlap.
        try testing.expectEqual(end, subresource.element_offset);
        end = subresource.element_offset + subresource.element_count;
        try testing.expect(end <= buffer_size);
    }

    try testing.expectEqual(buffer_size, end);

    // One pass's slice holds every meshlet of every pass at worst case.
    const worst_case_per_pass = @as(u64, max_visible_meshlets_per_pass) *
        hlsl_meshlet.MeshletMaxTriangleCount * triangle_indices_size_bytes;
    try testing.expectEqual(
        worst_case_per_pass * max_meshlet_culling_pass_count,
        visible_index_buffer_size_bytes,
    );
}
