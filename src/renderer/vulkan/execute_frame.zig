// Port of the frame driver in src/renderer/vulkan/renderpass/TestGraphics.cpp
// (resize_swapchain() and backend_execute_frame()).
//
// M0 records a bare vkCmdClearColorImage instead of the real frame graph; the
// surrounding sync — timeline wait, acquire with retries, submit2, present —
// is already the final shape.

const std = @import("std");
const vk = @import("vulkan");
const log = std.log.scoped(.vulkan);

const barrier = @import("barrier.zig");
const debug_geometry = @import("renderpass/debug_geometry.zig");
const descriptor_set = @import("descriptor_set.zig");
const exposure = @import("renderpass/exposure.zig");
const fg = @import("../graph/frame_graph.zig");
const forward = @import("renderpass/forward.zig");
const gbuffer = @import("renderpass/gbuffer.zig");
const gpu_scope = @import("gpu_scope.zig");
const frame_graph_pass = @import("renderpass/frame_graph_pass.zig");
const gui = @import("renderpass/gui.zig");
const imgui = @import("../imgui.zig");
const histogram = @import("renderpass/histogram.zig");
const lighting = @import("renderpass/lighting.zig");
const hzb = @import("renderpass/hzb.zig");
const shadow_map = @import("renderpass/shadow_map.zig");
const tiled_lighting = @import("renderpass/tiled_lighting.zig");
const tiled_lighting_common = @import("renderpass/tiled_lighting_common.zig");
const tiled_raster = @import("renderpass/tiled_raster.zig");
const vis_buffer = @import("renderpass/vis_buffer.zig");
const material_resources_module = @import("material_resources.zig");
const meshlet_culling = @import("renderpass/meshlet_culling.zig");
const prepare_buckets = @import("../prepare_buckets.zig");
const scene_module = @import("../scene.zig");
const screenshot = @import("screenshot.zig");
const swapchain_pass = @import("renderpass/swapchain_pass.zig");
const tone_mapping = @import("renderpass/tone_mapping.zig");
const Builder = @import("../graph/builder.zig").Builder;
const Swapchain = @import("Swapchain.zig");
const BackendResources = @import("backend_resources.zig").BackendResources;
const VulkanBackend = @import("Backend.zig").VulkanBackend;

/// vkAcquireNextImageKHR can legitimately answer NOT_READY; the C++ code
/// spins on it a bounded number of times before giving up on the frame.
const max_acquire_try_count = 10;

const acquire_timeout_ns: u64 = 1_000_000_000;
const timeline_wait_timeout_ns: u64 = 1_000_000_000;

// The swapchain image accesses from TestGraphics.cpp:353-363.
const swapchain_access_initial = barrier.GPUTextureAccess{
    .stage_mask = .{ .bottom_of_pipe_bit = true },
    .access_mask = .{},
    .image_layout = .undefined,
};

const swapchain_access_present = barrier.GPUTextureAccess{
    .stage_mask = .{ .bottom_of_pipe_bit = true },
    .access_mask = .{},
    .image_layout = .present_src_khr,
};

// The swapchain pass renders into the swapchain image directly, so the layout
// matches the C++ swapchain_access_render exactly.
const swapchain_access_render = barrier.GPUTextureAccess{
    .stage_mask = .{ .color_attachment_output_bit = true },
    .access_mask = .{ .color_attachment_write_bit = true },
    .image_layout = .attachment_optimal,
};

/// Mirrors the ImGui font upload in renderer_start() (ExecuteFrame.cpp): a
/// one-shot command buffer that stages the font atlas, submitted and waited on
/// synchronously before the first frame.
pub fn uploadImGuiFonts(backend: *VulkanBackend, resources: *BackendResources) !void {
    const device = backend.device;
    const vkd = backend.vkd;
    const cmd_buffer = resources.gfx_cmd_buffer.handle;

    try vkd.resetCommandPool(device, resources.gfx_command_pool, .{});

    const begin_info = vk.CommandBufferBeginInfo{
        .s_type = .command_buffer_begin_info,
        .p_next = null,
        .flags = .{ .one_time_submit_bit = true },
        .p_inheritance_info = null,
    };

    try vkd.beginCommandBuffer(cmd_buffer, &begin_info);

    try imgui.vulkanCreateFontsTexture(cmd_buffer);

    try vkd.endCommandBuffer(cmd_buffer);

    const command_buffer_info = vk.CommandBufferSubmitInfo{
        .s_type = .command_buffer_submit_info,
        .p_next = null,
        .command_buffer = cmd_buffer,
        // NOTE: set to zero when not using device groups
        .device_mask = 0,
    };

    const submit_info_2 = [_]vk.SubmitInfo2{.{
        .s_type = .submit_info_2,
        .p_next = null,
        .flags = .{},
        .wait_semaphore_info_count = 0,
        .p_wait_semaphore_infos = undefined,
        .command_buffer_info_count = 1,
        .p_command_buffer_infos = @ptrCast(&command_buffer_info),
        .signal_semaphore_info_count = 0,
        .p_signal_semaphore_infos = undefined,
    }};

    try vkd.queueSubmit2(backend.graphics_queue, &submit_info_2, .null_handle);
    try vkd.queueWaitIdle(backend.graphics_queue);

    imgui.vulkanDestroyFontUploadObjects();
}

/// Mirrors resize_swapchain(). This is the consumer of new_swapchain_extent —
/// the old Zig port never called it, so the window could not be resized.
pub fn resizeSwapchain(backend: *VulkanBackend) !void {
    if (backend.new_swapchain_extent.width == 0 and backend.new_swapchain_extent.height == 0) {
        return;
    }

    // FIXME Same coarse sync as the C++ side.
    try backend.vkd.queueWaitIdle(backend.present_queue);

    std.debug.assert(backend.new_swapchain_extent.width > 0);
    std.debug.assert(backend.new_swapchain_extent.height > 0);

    try Swapchain.resizeVulkanWmSwapchain(
        backend.vki,
        backend.vkd,
        backend.device,
        backend.physical_device.handle,
        &backend.present_info,
        backend.new_swapchain_extent,
        backend.allocator,
    );

    backend.updateRenderExtent();

    backend.new_swapchain_extent = .{ .width = 0, .height = 0 };
}

/// Mirrors backend_execute_frame(). Returns the swapchain image index that was
/// presented, or null when the frame was dropped because the swapchain went out
/// of date — the caller just runs another iteration.
pub fn executeFrame(
    backend: *VulkanBackend,
    resources: *BackendResources,
    scene: *const scene_module.Scene,
    readback: ?*const screenshot.Readback,
) !?u32 {
    const device = backend.device;
    const vkd = backend.vkd;

    // Mirrors backend_execute_frame(): the factory builds any pipeline that
    // has not been created yet before anything is recorded.
    try resources.pipeline_factory.update(&vkd, device);

    // ---- Wait for the GPU to be done with frame N-1 ----
    {
        const frame_index_to_wait = backend.frame_index;

        const timeline_semaphore_wait_info = vk.SemaphoreWaitInfo{
            .s_type = .semaphore_wait_info,
            .p_next = null,
            .flags = .{},
            .semaphore_count = 1,
            .p_semaphores = @ptrCast(&resources.frame_sync_resources.timeline_semaphore),
            .p_values = @ptrCast(&frame_index_to_wait),
        };

        while (true) {
            const wait_result = try vkd.waitSemaphores(device, &timeline_semaphore_wait_info, timeline_wait_timeout_ns);
            if (wait_result != .timeout) break;
            log.debug("timeline semaphore wait timed out, retrying", .{});
        }
    }

    backend.frame_index += 1;

    _ = resources.frame_arena.reset(.retain_capacity);

    // ---- Acquire ----
    var acquire = vk.DeviceWrapper.AcquireNextImageKHRResult{ .result = .not_ready, .image_index = 0 };

    for (0..max_acquire_try_count) |_| {
        acquire = vkd.acquireNextImageKHR(
            device,
            backend.present_info.swapchain,
            acquire_timeout_ns,
            backend.semaphore_swapchain_image_available,
            .null_handle,
        ) catch |err| switch (err) {
            // The window can change state between event handling and acquire;
            // drop the frame and let the resize path rebuild the swapchain.
            error.OutOfDateKHR => {
                log.warn("acquire out of date", .{});
                backend.new_swapchain_extent = backend.present_info.surface_extent;
                return null;
            },
            else => return err,
        };

        if (acquire.result != .not_ready) break;
    }

    switch (acquire.result) {
        .success => {},
        .not_ready => log.debug("acquire still not ready after {} tries", .{max_acquire_try_count}),
        .suboptimal_khr => log.debug("acquire suboptimal", .{}),
        .timeout => {
            log.warn("acquire timed out", .{});
            return null;
        },
        else => unreachable,
    }

    const current_swapchain_index = acquire.image_index;

    // ---- Record ----
    const cmd_buffer = resources.gfx_cmd_buffer.handle;

    try vkd.resetCommandPool(device, resources.gfx_command_pool, .{});

    const cmd_buffer_begin_info = vk.CommandBufferBeginInfo{
        .s_type = .command_buffer_begin_info,
        .p_next = null,
        .flags = .{ .one_time_submit_bit = true },
        .p_inheritance_info = null,
    };

    try vkd.beginCommandBuffer(cmd_buffer, &cmd_buffer_begin_info);

    // Wraps every pass below, so a capture and a Tracy trace both open on one
    // collapsible frame rather than a flat list of scopes.
    //
    // Closed explicitly rather than with `defer`: a defer here fires at
    // function exit, which is AFTER vkEndCommandBuffer, and recording a label
    // into an already-ended buffer is a validation error. An early error return
    // does leave the label open, but that path abandons the command buffer
    // without submitting it, so nothing ever sees the imbalance.
    const frame_scope = gpu_scope.begin(vkd, cmd_buffer, @src(), "GPU Frame");

    const subresource = barrier.defaultTextureSubresourceOneColorMip();
    const swapchain_image = backend.present_info.images[current_swapchain_index];

    // ---- Prepare the scene ----
    //
    // Declare, build, allocate volatile resources, update descriptors,
    // schedule, then record — the same order as backend_execute_frame.
    const frame_allocator = resources.frame_arena.allocator();
    const render_extent = backend.present_info.surface_extent;

    var prepared = prepare_buckets.PreparedData{};
    try prepare_buckets.prepareScene(
        frame_allocator,
        &scene.graph,
        &prepared,
        scene.mesh_allocs.items,
        scene_module.buildCamera(scene, render_extent),
        0,
    );

    var tiled_lighting_frame = tiled_lighting_common.TiledLightingFrame{};
    try tiled_lighting_common.prepareTileLightingFrame(
        frame_allocator,
        &scene.graph,
        scene_module.buildCamera(scene, render_extent),
        &tiled_lighting_frame,
    );

    const tile_extent = vk.Extent2D{
        .width = tiled_lighting_frame.tile_count_x,
        .height = tiled_lighting_frame.tile_count_y,
    };

    // ---- Declare the frame graph ----
    var framegraph = fg.FrameGraph{};
    var builder = Builder.init(&framegraph, frame_allocator);

    const tone_map_record = try tone_mapping.createFrameGraphRecord(&builder, false);
    const meshlet_record = try meshlet_culling.createFrameGraphRecord(&builder);

    const debug_geometry_start_record = try debug_geometry.createStartFrameGraphRecord(&builder);

    const shadow_record = try shadow_map.createFrameGraphRecord(
        &builder,
        frame_allocator,
        meshlet_record,
        &prepared,
    );

    // Flipped by the "Enable MSAA-based visibility" checkbox in the Rendering
    // window; defaults off.
    const enable_msaa = backend.options.enable_msaa_visibility;
    const support_shader_stores_to_depth = backend.physical_device.macro_features.compute_stores_to_depth;

    const vis_buffer_record = try vis_buffer.createFrameGraphRecord(
        &builder,
        meshlet_record,
        render_extent,
        enable_msaa,
        support_shader_stores_to_depth,
    );

    // Flipped by the "Raster G-buffer" checkbox. Read once: the declaration
    // below and the record call further down have to agree, because the frame
    // graph requires passes to be recorded in declaration order.
    const use_raster_gbuffer = backend.options.use_raster_gbuffer;

    const gbuffer_record: ?gbuffer.GBufferFrameGraphRecord = if (use_raster_gbuffer)
        try gbuffer.createFrameGraphRecord(&builder, meshlet_record, vis_buffer_record, render_extent)
    else
        null;

    const hzb_record = try hzb.createFrameGraphRecord(
        &builder,
        frame_allocator,
        vis_buffer_record.depth,
        &tiled_lighting_frame,
    );

    const light_raster_record = try tiled_raster.createFrameGraphRecord(
        &builder,
        &tiled_lighting_frame,
        hzb_record.hzb_properties,
        hzb_record.hzb_texture,
    );

    const tiled_lighting_record = try tiled_lighting.createFrameGraphRecord(
        &builder,
        frame_allocator,
        vis_buffer_record,
        if (gbuffer_record) |record| record.gbuffer_rt0 else vis_buffer_record.fill_gbuffer.gbuffer_rt0,
        if (gbuffer_record) |record| record.gbuffer_rt1 else vis_buffer_record.fill_gbuffer.gbuffer_rt1,
        shadow_record,
        light_raster_record,
    );

    const tiled_lighting_debug_record = try tiled_lighting.createDebugFrameGraphRecord(
        &builder,
        tiled_lighting_record,
        render_extent,
    );

    const forward_record = try forward.createFrameGraphRecord(
        &builder,
        frame_allocator,
        meshlet_record,
        shadow_record.shadow_maps,
        vis_buffer_record.depth,
        render_extent,
    );

    const gui_record = try gui.createFrameGraphRecord(&builder, backend.present_info.surface_extent);

    const histogram_clear_record = try histogram.createClearFrameGraphRecord(&builder);
    const histogram_record = try histogram.createFrameGraphRecord(
        &builder,
        histogram_clear_record,
        forward_record.scene_hdr,
    );

    const exposure_record = try exposure.createFrameGraphRecord(&builder, forward_record.scene_hdr, render_extent);

    // NOTE: if you have GPU passes that write debug commands, the last one
    // should give its handles here.
    const last_draw_count_handle = debug_geometry_start_record.draw_counter;
    const last_user_commands_buffer_handle = debug_geometry_start_record.user_commands_buffer;

    const debug_geometry_build_cmds_record = try debug_geometry.createComputeFrameGraphRecord(
        &builder,
        last_draw_count_handle,
        last_user_commands_buffer_handle,
    );

    const debug_geometry_draw_record = try debug_geometry.createDrawFrameGraphRecord(
        &builder,
        debug_geometry_build_cmds_record,
        last_draw_count_handle,
        tiled_lighting_record.lighting,
        forward_record.depth,
    );

    const swapchain_record = try swapchain_pass.createFrameGraphRecord(
        &builder,
        forward_record.scene_hdr,
        .{
            // The debug geometry draws over the lit image, so the composite
            // samples its output rather than the tiled lighting pass's.
            .lighting_result = debug_geometry_draw_record.scene_hdr,
            .tile_debug = tiled_lighting_debug_record.output,
            .gui = gui_record.output,
            .histogram = histogram_record.histogram_buffer,
            .average_exposure = exposure_record.reduce_tail.average_exposure,
        },
        tone_map_record.tone_map_lut,
    );

    try builder.build();

    try resources.framegraph_resources.allocateVolatileResources(
        vkd,
        device,
        backend.vma_instance,
        &framegraph,
    );

    // ---- Update descriptors ----
    lighting.uploadFrameResources(&resources.frame_storage_allocator, &prepared, &resources.lighting_resources);

    {
        // 200/200 matches the C++; the vis buffer and tiled lighting passes
        // alone write more than the 64 the forward-only frame needed.
        var write_helper = try descriptor_set.DescriptorWriteHelper.init(frame_allocator, 200, 200, 1);
        defer write_helper.deinit();

        tone_mapping.updateDescriptorSet(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            tone_map_record,
            &resources.tone_map_pass_resources,
        );

        meshlet_culling.updatePassesResources(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            meshlet_record,
            &resources.frame_storage_allocator,
            &prepared,
            &resources.meshlet_culling_resources,
            &resources.mesh_cache,
        );

        shadow_map.updateDescriptorSets(
            &write_helper,
            &resources.frame_storage_allocator,
            &prepared,
            &resources.shadow_map_resources,
            resources.mesh_cache.vertex_buffer_position.handle,
        );

        try forward.updateDescriptorSets(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            forward_record,
            &resources.frame_storage_allocator,
            &prepared,
            &resources.forward_pass_resources,
            resources.sampler_resources,
            &resources.mesh_cache,
            &resources.material_resources,
            resources.lighting_resources,
            backend.vma_instance,
        );

        if (gbuffer_record) |record| {
            try gbuffer.updateDescriptorSets(
                &write_helper,
                &framegraph,
                &resources.framegraph_resources,
                record,
                &resources.frame_storage_allocator,
                &prepared,
                &resources.gbuffer_pass_resources,
                resources.sampler_resources,
                &resources.mesh_cache,
                &resources.material_resources,
                backend.vma_instance,
            );
        }

        vis_buffer.updateDescriptorSets(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            vis_buffer_record,
            &resources.frame_storage_allocator,
            &resources.vis_buffer_pass_resources,
            &prepared,
            resources.sampler_resources,
            &resources.material_resources,
            &resources.mesh_cache,
            enable_msaa,
            support_shader_stores_to_depth,
        );

        hzb.updateDescriptorSet(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            hzb_record,
            &resources.hzb_pass_resources,
            resources.sampler_resources,
        );

        tiled_raster.updateDescriptorSets(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            light_raster_record,
            &resources.frame_storage_allocator,
            &resources.tiled_raster_resources,
            &tiled_lighting_frame,
        );

        try tiled_lighting.updateDescriptorSets(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            tiled_lighting_record,
            &prepared,
            resources.lighting_resources,
            &resources.tiled_lighting_pass_resources,
            resources.sampler_resources,
            backend.vma_instance,
        );

        tiled_lighting.updateDebugDescriptorSet(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            tiled_lighting_debug_record,
            &resources.tiled_lighting_pass_resources,
        );

        histogram.updateDescriptorSet(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            histogram_record,
            &resources.histogram_pass_resources,
            resources.sampler_resources,
        );

        exposure.updateDescriptorSets(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            exposure_record,
            &resources.exposure_pass_resources,
            resources.sampler_resources,
        );

        try debug_geometry.updateStartResources(
            backend.vma_instance,
            &prepared,
            &resources.debug_geometry_resources,
        );

        try debug_geometry.updateBuildCmdsResources(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            debug_geometry_build_cmds_record,
            &prepared,
            &resources.debug_geometry_resources,
            backend.vma_instance,
        );

        debug_geometry.updateDrawDescriptorSets(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            debug_geometry_draw_record,
            &resources.debug_geometry_resources,
        );

        swapchain_pass.updateDescriptorSet(
            &write_helper,
            &framegraph,
            &resources.framegraph_resources,
            swapchain_record,
            &resources.swapchain_pass_resources,
            resources.sampler_resources,
        );

        write_helper.flush(vkd, device);
    }

    resources.frame_storage_allocator.commitToGpu(vkd, device, backend.vma_instance);

    var schedule = try fg.computeSchedule(&framegraph, frame_allocator);
    defer schedule.deinit(frame_allocator);

    // ---- Record ----

    // An image straight out of a freshly created swapchain is in UNDEFINED
    // layout; every later frame gets it back in PRESENT_SRC.
    const pending_initial_transition = &backend.present_info.images_pending_initial_transition[current_swapchain_index];
    const src_access = if (pending_initial_transition.*) swapchain_access_initial else swapchain_access_present;
    pending_initial_transition.* = false;

    {
        const scope = gpu_scope.begin(vkd, cmd_buffer, @src(), "Barrier");
        defer scope.end(vkd, cmd_buffer);

        const image_barriers = [_]vk.ImageMemoryBarrier2{
            barrier.getImageBarrierSameQueue(swapchain_image, subresource, src_access, swapchain_access_render),
        };

        const dependencies = barrier.getImageBarrierDependencyInfo(&image_barriers);
        vkd.cmdPipelineBarrier2(cmd_buffer, &dependencies);
    }

    try material_resources_module.recordUploadCommandBuffer(
        vkd,
        cmd_buffer,
        &resources.material_resources,
        frame_allocator,
    );

    const frame_graph_helper = frame_graph_pass.FrameGraphHelper{
        .frame_graph = &framegraph,
        .schedule = &schedule,
        .resources = &resources.framegraph_resources,
    };

    // NOTE: passes must be RECORDED in the order they were DECLARED. The
    // automatic barriers are placed against render pass indices, so recording
    // out of order leaves images in the wrong layout at submit time.
    tone_mapping.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        tone_map_record,
        &resources.tone_map_pass_resources,
        backend.present_info.tonemap_min_nits,
        backend.present_info.tonemap_max_nits,
    );

    meshlet_culling.recordClearCommandBuffer(vkd, cmd_buffer, frame_graph_helper, meshlet_record.clear);
    meshlet_culling.recordCullMeshletsCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        meshlet_record.cull_meshlets,
        &prepared,
        &resources.meshlet_culling_resources,
    );
    meshlet_culling.recordCullTrianglesPrepareCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        meshlet_record.cull_triangles_prepare,
        &prepared,
        &resources.meshlet_culling_resources,
    );
    meshlet_culling.recordCullTrianglesCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        meshlet_record.cull_triangles,
        &prepared,
        &resources.meshlet_culling_resources,
    );

    meshlet_culling.recordDebugCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        meshlet_record.debug,
        &resources.meshlet_culling_resources,
    );

    debug_geometry.recordStartCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        debug_geometry_start_record,
        &prepared,
        &resources.debug_geometry_resources,
    );

    shadow_map.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        shadow_record,
        &prepared,
        &resources.shadow_map_resources,
    );

    vis_buffer.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        vis_buffer_record.render,
        &prepared,
        &resources.vis_buffer_pass_resources,
        enable_msaa,
    );

    vis_buffer.recordFillGBufferCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        vis_buffer_record.fill_gbuffer,
        &resources.vis_buffer_pass_resources,
        render_extent,
        enable_msaa,
        support_shader_stores_to_depth,
    );

    vis_buffer.recordLegacyDepthResolveCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        vis_buffer_record.legacy_depth_resolve,
        &resources.vis_buffer_pass_resources,
        enable_msaa,
        support_shader_stores_to_depth,
    );

    if (gbuffer_record) |record| {
        gbuffer.recordCommandBuffer(
            vkd,
            cmd_buffer,
            frame_graph_helper,
            &resources.pipeline_factory,
            record,
            &prepared,
            &resources.gbuffer_pass_resources,
        );
    }

    hzb.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        hzb_record,
        &resources.hzb_pass_resources,
        render_extent,
        .{ .width = hzb_record.hzb_properties.width, .height = hzb_record.hzb_properties.height },
    );

    tiled_raster.recordDepthCopy(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        light_raster_record.tile_depth_copy,
        &resources.tiled_raster_resources,
    );

    tiled_raster.recordLightClassifyCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        light_raster_record.light_classify,
        &tiled_lighting_frame,
        &resources.tiled_raster_resources,
    );

    tiled_raster.recordLightRasterCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        light_raster_record.light_raster,
        resources.tiled_raster_resources.light_raster,
    );

    tiled_lighting.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        tiled_lighting_record,
        &resources.tiled_lighting_pass_resources,
        render_extent,
        tile_extent,
    );

    tiled_lighting.recordDebugCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        tiled_lighting_debug_record,
        &resources.tiled_lighting_pass_resources,
        render_extent,
        tile_extent,
    );

    forward.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        forward_record,
        &prepared,
        &resources.forward_pass_resources,
    );

    gui.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        gui_record,
        &resources.gui_pass_resources,
    );

    histogram.recordClearCommandBuffer(vkd, cmd_buffer, frame_graph_helper, histogram_clear_record);

    histogram.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        histogram_record,
        &resources.histogram_pass_resources,
        render_extent,
    );

    exposure.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        exposure_record,
        &resources.exposure_pass_resources,
    );

    debug_geometry.recordBuildCmdsCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        debug_geometry_build_cmds_record,
        &resources.debug_geometry_resources,
    );

    debug_geometry.recordDrawCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        &resources.pipeline_factory,
        debug_geometry_draw_record,
        &resources.debug_geometry_resources,
    );

    swapchain_pass.recordCommandBuffer(
        vkd,
        cmd_buffer,
        frame_graph_helper,
        swapchain_record,
        &resources.swapchain_pass_resources,
        backend.present_info.image_views[current_swapchain_index],
        render_extent,
        .{
            .exposure_compensation_stops = backend.present_info.exposure_compensation_stops,
            .tonemap_min_nits = backend.present_info.tonemap_min_nits,
            .tonemap_max_nits = backend.present_info.tonemap_max_nits,
            .sdr_ui_max_brightness_nits = backend.present_info.sdr_ui_max_brightness_nits,
            .sdr_peak_brightness_nits = backend.present_info.sdr_peak_brightness_nits,
        },
    );

    // Back to PRESENT_SRC before handing the image to the presentation engine.
    if (readback) |readback_target| {
        // The readback has to happen here, while the frame still owns the
        // image: transitioning an image that has already been presented is
        // invalid.
        const to_copy = [_]vk.ImageMemoryBarrier2{
            barrier.getImageBarrierSameQueue(swapchain_image, subresource, swapchain_access_render, screenshot.access_copy),
        };
        vkd.cmdPipelineBarrier2(cmd_buffer, &barrier.getImageBarrierDependencyInfo(&to_copy));

        screenshot.recordCopy(vkd, cmd_buffer, swapchain_image, readback_target);
    }

    {
        const scope = gpu_scope.begin(vkd, cmd_buffer, @src(), "Barrier");
        defer scope.end(vkd, cmd_buffer);

        const src_layout = if (readback != null) screenshot.access_copy else swapchain_access_render;

        const image_barriers = [_]vk.ImageMemoryBarrier2{
            barrier.getImageBarrierSameQueue(swapchain_image, subresource, src_layout, swapchain_access_present),
        };

        const dependencies = barrier.getImageBarrierDependencyInfo(&image_barriers);
        vkd.cmdPipelineBarrier2(cmd_buffer, &dependencies);
    }

    frame_scope.end(vkd, cmd_buffer);

    try vkd.endCommandBuffer(cmd_buffer);

    // ---- Submit ----
    const wait_semaphore_info = vk.SemaphoreSubmitInfo{
        .s_type = .semaphore_submit_info,
        .p_next = null,
        .semaphore = backend.semaphore_swapchain_image_available,
        .value = 0, // NOTE: only meaningful for timeline semaphores
        .stage_mask = swapchain_access_present.stage_mask,
        .device_index = 0,
    };

    const command_buffer_info = vk.CommandBufferSubmitInfo{
        .s_type = .command_buffer_submit_info,
        .p_next = null,
        .command_buffer = cmd_buffer,
        .device_mask = 0,
    };

    const signal_semaphore_info = [_]vk.SemaphoreSubmitInfo{
        .{
            .s_type = .semaphore_submit_info,
            .p_next = null,
            .semaphore = backend.present_info.semaphores_rendering_finished[current_swapchain_index],
            .value = 0,
            .stage_mask = .{ .all_commands_bit = true },
            .device_index = 0,
        },
        .{
            .s_type = .semaphore_submit_info,
            .p_next = null,
            .semaphore = resources.frame_sync_resources.timeline_semaphore,
            .value = backend.frame_index,
            .stage_mask = .{ .all_commands_bit = true },
            .device_index = 0,
        },
    };

    const submit_info_2 = [_]vk.SubmitInfo2{.{
        .s_type = .submit_info_2,
        .p_next = null,
        .flags = .{},
        .wait_semaphore_info_count = 1,
        .p_wait_semaphore_infos = @ptrCast(&wait_semaphore_info),
        .command_buffer_info_count = 1,
        .p_command_buffer_infos = @ptrCast(&command_buffer_info),
        .signal_semaphore_info_count = signal_semaphore_info.len,
        .p_signal_semaphore_infos = &signal_semaphore_info,
    }};

    try vkd.queueSubmit2(backend.graphics_queue, &submit_info_2, .null_handle);

    // ---- Present ----
    const present_info = vk.PresentInfoKHR{
        .s_type = .present_info_khr,
        .p_next = null,
        .wait_semaphore_count = 1,
        .p_wait_semaphores = @ptrCast(&backend.present_info.semaphores_rendering_finished[current_swapchain_index]),
        .swapchain_count = 1,
        .p_swapchains = @ptrCast(&backend.present_info.swapchain),
        .p_image_indices = @ptrCast(&current_swapchain_index),
        .p_results = null,
    };

    const present_result = vkd.queuePresentKHR(backend.present_queue, &present_info) catch |err| switch (err) {
        // The window can change state between event handling and presenting,
        // so this is expected as long as we rebuild a correct swapchain later.
        error.OutOfDateKHR => {
            log.warn("present out of date", .{});
            backend.new_swapchain_extent = backend.present_info.surface_extent;
            return null;
        },
        else => return err,
    };

    if (present_result == .suboptimal_khr) {
        backend.new_swapchain_extent = backend.present_info.surface_extent;
        log.warn("present suboptimal, requesting swapchain re-creation", .{});
    } else {
        std.debug.assert(present_result == .success);
    }

    if (backend.frame_index == log_stats_on_frame) {
        logCullingStats(backend, resources, prepared.cull_passes.items.len) catch {};
    }

    return current_swapchain_index;
}

/// The counters are only valid once the GPU is done with the frame, so this
/// waits — fine for a one-shot debug log, not something to do every frame.
const log_stats_on_frame = 8;

fn logCullingStats(backend: *VulkanBackend, resources: *BackendResources, pass_count: usize) !void {
    try backend.vkd.queueWaitIdle(backend.graphics_queue);

    var stats_buffer: [meshlet_culling.max_meshlet_culling_pass_count]meshlet_culling.MeshletCullingStats = undefined;

    const stats = try meshlet_culling.getGpuStats(
        backend.vkd,
        backend.device,
        backend.vma_instance,
        &resources.meshlet_culling_resources,
        backend.physical_device.properties.limits.non_coherent_atom_size,
        pass_count,
        &stats_buffer,
    );

    log.info("GPU mesh culling stats:", .{});
    for (stats) |s| {
        log.info("- pass {}: {} meshlets, {} triangles, {} draw commands", .{
            s.pass_index,
            s.surviving_meshlet_count,
            s.surviving_triangle_count,
            s.indirect_draw_command_count,
        });
    }
}
