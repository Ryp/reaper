// Port of src/renderer/vulkan/BackendResources.h + BackendResources.cpp
//
// Everything the backend needs that outlives a frame but is not part of the
// device itself. The C++ version creates ~25 sub-resource sets here; they get
// added milestone by milestone, in the same order as the original.

const std = @import("std");
const vk = @import("vulkan");

const CommandBuffer = @import("command_buffer.zig").CommandBuffer;
const debug_geometry = @import("renderpass/debug_geometry.zig");
const debug_gradient = @import("renderpass/debug_gradient.zig");
const exposure = @import("renderpass/exposure.zig");
const frame_sync = @import("frame_sync.zig");
const forward = @import("renderpass/forward.zig");
const gbuffer = @import("renderpass/gbuffer.zig");
const gui = @import("renderpass/gui.zig");
const histogram = @import("renderpass/histogram.zig");
const lighting = @import("renderpass/lighting.zig");
const shadow_map = @import("renderpass/shadow_map.zig");
const tiled_lighting = @import("renderpass/tiled_lighting.zig");
const tiled_raster = @import("renderpass/tiled_raster.zig");
const vis_buffer = @import("renderpass/vis_buffer.zig");
const hzb = @import("renderpass/hzb.zig");
const meshlet_culling = @import("renderpass/meshlet_culling.zig");
const swapchain_pass = @import("renderpass/swapchain_pass.zig");
const tone_mapping = @import("renderpass/tone_mapping.zig");
const vma = @import("vma.zig").c;
const FrameGraphResources = @import("framegraph_resources.zig").FrameGraphResources;
const MaterialResources = @import("material_resources.zig").MaterialResources;
const MeshCache = @import("mesh_cache.zig").MeshCache;
const PipelineFactory = @import("pipeline_factory.zig").PipelineFactory;
const SamplerResources = @import("sampler_resources.zig").SamplerResources;
const StorageBufferAllocator = @import("storage_buffer.zig").StorageBufferAllocator;

/// The device limits and handles BackendResources needs but does not own.
/// Passed explicitly rather than taking the whole backend, so this stays
/// independent of Backend.zig.
pub const InitParams = struct {
    graphics_queue_family_index: u32,
    descriptor_pool: vk.DescriptorPool,
    vma_instance: vma.VmaAllocator,
    max_draw_indirect_count: u32,
    min_storage_buffer_offset_alignment: u64,
    /// The tiled raster pass loads its proxy volume mesh from disk at init.
    io: std.Io,
};

/// Matches the C++ frame storage allocator size.
const frame_storage_size_bytes: u64 = 1 << 20;
const log = std.log.scoped(.vulkan);

pub const BackendResources = struct {
    gfx_command_pool: vk.CommandPool,
    gfx_cmd_buffer: CommandBuffer,

    frame_sync_resources: frame_sync.FrameSyncResources,
    framegraph_resources: FrameGraphResources,
    pipeline_factory: PipelineFactory,
    sampler_resources: SamplerResources,
    frame_storage_allocator: StorageBufferAllocator,
    mesh_cache: MeshCache,
    material_resources: MaterialResources,
    debug_gradient_resources: debug_gradient.Resources,
    meshlet_culling_resources: meshlet_culling.MeshletCullingResources,
    forward_pass_resources: forward.ForwardPassResources,
    gbuffer_pass_resources: gbuffer.GBufferPassResources,
    shadow_map_resources: shadow_map.ShadowMapResources,
    vis_buffer_pass_resources: vis_buffer.VisibilityBufferPassResources,
    hzb_pass_resources: hzb.HZBPassResources,
    tiled_raster_resources: tiled_raster.TiledRasterResources,
    tiled_lighting_pass_resources: tiled_lighting.TiledLightingPassResources,
    lighting_resources: lighting.LightingPassResources,
    histogram_pass_resources: histogram.HistogramPassResources,
    exposure_pass_resources: exposure.ExposurePassResources,
    debug_geometry_resources: debug_geometry.DebugGeometryPassResources,
    gui_pass_resources: gui.GuiPassResources,
    swapchain_pass_resources: swapchain_pass.SwapchainPassResources,
    tone_map_pass_resources: tone_mapping.ToneMapPassResources,

    /// Reset with .retain_capacity at the top of every frame; all
    /// frame-lifetime allocations come from here.
    frame_arena: std.heap.ArenaAllocator,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        params: InitParams,
        swapchain_format: @import("Swapchain.zig").SwapchainFormat,
        allocator: std.mem.Allocator,
    ) !BackendResources {
        const descriptor_pool = params.descriptor_pool;
        const pool_create_info = vk.CommandPoolCreateInfo{
            .s_type = .command_pool_create_info,
            .p_next = null,
            .flags = .{},
            .queue_family_index = params.graphics_queue_family_index,
        };

        const gfx_command_pool = try vkd.createCommandPool(device, &pool_create_info, null);
        errdefer vkd.destroyCommandPool(device, gfx_command_pool, null);

        log.debug("created command pool", .{});

        const cmd_buffer_alloc_info = vk.CommandBufferAllocateInfo{
            .s_type = .command_buffer_allocate_info,
            .p_next = null,
            .command_pool = gfx_command_pool,
            .level = .primary,
            .command_buffer_count = 1,
        };

        var cmd_buffer_handle: vk.CommandBuffer = .null_handle;
        try vkd.allocateCommandBuffers(device, &cmd_buffer_alloc_info, @ptrCast(&cmd_buffer_handle));

        log.debug("created command buffer", .{});

        const frame_sync_resources = try frame_sync.create(vkd, device);
        errdefer frame_sync.destroy(vkd, device, frame_sync_resources);

        var framegraph_resources = try FrameGraphResources.init(vkd, device, allocator);
        errdefer framegraph_resources.deinit(vkd, device, null);

        var pipeline_factory = PipelineFactory.init(allocator);
        errdefer pipeline_factory.deinit(vkd, device);

        var sampler_resources = try SamplerResources.init(vkd, device);
        errdefer sampler_resources.deinit(vkd, device);

        var frame_storage_allocator = try StorageBufferAllocator.init(
            params.vma_instance,
            frame_storage_size_bytes,
            params.min_storage_buffer_offset_alignment,
        );
        errdefer frame_storage_allocator.deinit(params.vma_instance);

        var mesh_cache = try MeshCache.init(params.vma_instance, allocator);
        errdefer mesh_cache.deinit(params.vma_instance);

        var material_resources = try MaterialResources.init(params.vma_instance, allocator);
        errdefer material_resources.deinit(vkd, device, params.vma_instance);

        const debug_gradient_resources = try debug_gradient.Resources.init(vkd, device, descriptor_pool);

        var meshlet_culling_resources = try meshlet_culling.MeshletCullingResources.init(
            vkd,
            device,
            descriptor_pool,
            params.vma_instance,
            &pipeline_factory,
            params.max_draw_indirect_count,
        );
        errdefer meshlet_culling_resources.deinit(vkd, device, params.vma_instance);

        var forward_pass_resources = try forward.ForwardPassResources.init(
            vkd,
            device,
            descriptor_pool,
            params.vma_instance,
            &pipeline_factory,
        );
        errdefer forward_pass_resources.deinit(vkd, device, params.vma_instance);

        var gbuffer_pass_resources = try gbuffer.GBufferPassResources.init(
            vkd,
            device,
            descriptor_pool,
            params.vma_instance,
            &pipeline_factory,
        );
        errdefer gbuffer_pass_resources.deinit(vkd, device, params.vma_instance);

        var shadow_map_resources = try shadow_map.ShadowMapResources.init(
            vkd,
            device,
            descriptor_pool,
            &pipeline_factory,
        );
        errdefer shadow_map_resources.deinit(vkd, device);

        var vis_buffer_pass_resources = try vis_buffer.VisibilityBufferPassResources.init(
            vkd,
            device,
            descriptor_pool,
            &pipeline_factory,
        );
        errdefer vis_buffer_pass_resources.deinit(vkd, device);

        var hzb_pass_resources = try hzb.HZBPassResources.init(vkd, device, descriptor_pool, &pipeline_factory);
        errdefer hzb_pass_resources.deinit(vkd, device);

        var tiled_raster_resources = try tiled_raster.TiledRasterResources.init(
            vkd,
            device,
            descriptor_pool,
            params.vma_instance,
            &pipeline_factory,
            allocator,
            params.io,
        );
        errdefer tiled_raster_resources.deinit(vkd, device, params.vma_instance);

        var tiled_lighting_pass_resources = try tiled_lighting.TiledLightingPassResources.init(
            vkd,
            device,
            descriptor_pool,
            params.vma_instance,
            &pipeline_factory,
        );
        errdefer tiled_lighting_pass_resources.deinit(vkd, device, params.vma_instance);

        var histogram_pass_resources = try histogram.HistogramPassResources.init(
            vkd,
            device,
            descriptor_pool,
            &pipeline_factory,
        );
        errdefer histogram_pass_resources.deinit(vkd, device);

        var exposure_pass_resources = try exposure.ExposurePassResources.init(
            vkd,
            device,
            descriptor_pool,
            &pipeline_factory,
        );
        errdefer exposure_pass_resources.deinit(vkd, device);

        var debug_geometry_resources = try debug_geometry.DebugGeometryPassResources.init(
            vkd,
            device,
            descriptor_pool,
            params.vma_instance,
            &pipeline_factory,
            allocator,
            params.io,
        );
        errdefer debug_geometry_resources.deinit(vkd, device, params.vma_instance);

        var gui_pass_resources = try gui.GuiPassResources.init(vkd, device, descriptor_pool, &pipeline_factory);
        errdefer gui_pass_resources.deinit(vkd, device);

        const swapchain_pass_resources = try swapchain_pass.SwapchainPassResources.init(
            vkd,
            device,
            descriptor_pool,
            swapchain_format,
        );

        const tone_map_pass_resources = try tone_mapping.ToneMapPassResources.init(
            vkd,
            device,
            descriptor_pool,
            &pipeline_factory,
        );

        return .{
            .gfx_command_pool = gfx_command_pool,
            .gfx_cmd_buffer = .{ .handle = cmd_buffer_handle },
            .frame_sync_resources = frame_sync_resources,
            .framegraph_resources = framegraph_resources,
            .pipeline_factory = pipeline_factory,
            .sampler_resources = sampler_resources,
            .frame_storage_allocator = frame_storage_allocator,
            .mesh_cache = mesh_cache,
            .material_resources = material_resources,
            .debug_gradient_resources = debug_gradient_resources,
            .meshlet_culling_resources = meshlet_culling_resources,
            .forward_pass_resources = forward_pass_resources,
            .gbuffer_pass_resources = gbuffer_pass_resources,
            .shadow_map_resources = shadow_map_resources,
            .vis_buffer_pass_resources = vis_buffer_pass_resources,
            .hzb_pass_resources = hzb_pass_resources,
            .tiled_raster_resources = tiled_raster_resources,
            .tiled_lighting_pass_resources = tiled_lighting_pass_resources,
            .lighting_resources = .{},
            .histogram_pass_resources = histogram_pass_resources,
            .exposure_pass_resources = exposure_pass_resources,
            .debug_geometry_resources = debug_geometry_resources,
            .gui_pass_resources = gui_pass_resources,
            .swapchain_pass_resources = swapchain_pass_resources,
            .tone_map_pass_resources = tone_map_pass_resources,
            .frame_arena = .init(allocator),
        };
    }

    pub fn deinit(self: *BackendResources, vkd: anytype, device: vk.Device, vma_instance: vma.VmaAllocator) void {
        self.frame_arena.deinit();

        self.tone_map_pass_resources.deinit(vkd, device);
        self.swapchain_pass_resources.deinit(vkd, device);
        self.gui_pass_resources.deinit(vkd, device);
        self.debug_geometry_resources.deinit(vkd, device, vma_instance);
        self.exposure_pass_resources.deinit(vkd, device);
        self.histogram_pass_resources.deinit(vkd, device);
        self.tiled_lighting_pass_resources.deinit(vkd, device, vma_instance);
        self.tiled_raster_resources.deinit(vkd, device, vma_instance);
        self.hzb_pass_resources.deinit(vkd, device);
        self.vis_buffer_pass_resources.deinit(vkd, device);
        self.shadow_map_resources.deinit(vkd, device);
        self.gbuffer_pass_resources.deinit(vkd, device, vma_instance);
        self.forward_pass_resources.deinit(vkd, device, vma_instance);
        self.meshlet_culling_resources.deinit(vkd, device, vma_instance);
        self.debug_gradient_resources.deinit(vkd, device);
        self.material_resources.deinit(vkd, device, vma_instance);
        self.mesh_cache.deinit(vma_instance);
        self.frame_storage_allocator.deinit(vma_instance);
        self.sampler_resources.deinit(vkd, device);
        self.pipeline_factory.deinit(vkd, device);
        self.framegraph_resources.deinit(vkd, device, vma_instance);

        frame_sync.destroy(vkd, device, self.frame_sync_resources);

        vkd.freeCommandBuffers(device, self.gfx_command_pool, &.{self.gfx_cmd_buffer.handle});
        vkd.destroyCommandPool(device, self.gfx_command_pool, null);
    }
};
