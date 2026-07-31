// Frame graph smoke test — not a port of anything on the C++ side.
//
// Three passes that need no scene, so the graph machinery can be exercised
// end to end before any real pass exists:
//
//   Gradient : compute writes a UV gradient into texture A (storage image)
//   Copy     : vkCmdCopyImage from A into texture B
//   Present  : blit B onto the swapchain image
//
// That covers resource declaration, read/write usages, automatic barrier
// placement across three passes with genuine layout transitions, split
// barriers, descriptor set updates and volatile resource allocation.
//
// Delete this once the real passes cover the same ground.

const std = @import("std");
const vk = @import("vulkan");

const barrier_module = @import("../barrier.zig");
const descriptor_set = @import("../descriptor_set.zig");
const fg = @import("../../graph/frame_graph.zig");
const frame_graph_pass = @import("frame_graph_pass.zig");
const hlsl = @import("../../hlsl/debug_gradient.zig");
const pipeline_module = @import("../pipeline.zig");
const shader_modules = @import("../shader_modules.zig");

const Builder = @import("../../graph/builder.zig").Builder;
const FrameGraphResources = @import("../framegraph_resources.zig").FrameGraphResources;
const gpu_texture_properties = @import("../../texture/gpu_texture_properties.zig");

const access_compute_write = barrier_module.GPUTextureAccess{
    .stage_mask = .{ .compute_shader_bit = true },
    .access_mask = .{ .shader_storage_write_bit = true },
    .image_layout = .general,
};

const access_transfer_src = barrier_module.GPUTextureAccess{
    .stage_mask = .{ .copy_bit = true },
    .access_mask = .{ .transfer_read_bit = true },
    .image_layout = .transfer_src_optimal,
};

const access_transfer_dst = barrier_module.GPUTextureAccess{
    .stage_mask = .{ .copy_bit = true },
    .access_mask = .{ .transfer_write_bit = true },
    .image_layout = .transfer_dst_optimal,
};

// Same layout as access_transfer_src but a different stage: vkCmdBlitImage runs
// in BLIT, not COPY, and a barrier that only makes the image available to COPY
// leaves a read-after-write hazard that synchronization validation flags.
const access_blit_src = barrier_module.GPUTextureAccess{
    .stage_mask = .{ .blit_bit = true },
    .access_mask = .{ .transfer_read_bit = true },
    .image_layout = .transfer_src_optimal,
};

pub const Resources = struct {
    descriptor_set_layout: vk.DescriptorSetLayout,
    pipeline_layout: vk.PipelineLayout,
    pipeline: vk.Pipeline,
    descriptor_set: vk.DescriptorSet,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        descriptor_pool: vk.DescriptorPool,
    ) !Resources {
        const bindings = [_]descriptor_set.DescriptorBinding{
            .{ .slot = 0, .count = 1, .type = .storage_image, .stage_mask = .{ .compute_bit = true } },
        };

        var layout_bindings: [bindings.len]vk.DescriptorSetLayoutBinding = undefined;
        descriptor_set.fillLayoutBindings(&layout_bindings, &bindings);

        const descriptor_set_layout = try pipeline_module.createDescriptorSetLayout(
            vkd,
            device,
            &layout_bindings,
            &.{},
        );
        errdefer vkd.destroyDescriptorSetLayout(device, descriptor_set_layout, null);

        const push_constant_ranges = [_]vk.PushConstantRange{.{
            .stage_flags = .{ .compute_bit = true },
            .offset = 0,
            .size = @sizeOf(hlsl.DebugGradientPushConstants),
        }};

        const pipeline_layout = try pipeline_module.createPipelineLayout(
            vkd,
            device,
            &.{descriptor_set_layout},
            &push_constant_ranges,
        );
        errdefer vkd.destroyPipelineLayout(device, pipeline_layout, null);

        const module_create_info = pipeline_module.shaderModuleCreateInfo(
            shader_modules.get("debug_gradient.comp.spv"),
        );

        const pipeline = try pipeline_module.createComputePipeline(
            vkd,
            device,
            pipeline_layout,
            pipeline_module.defaultPipelineShaderStageCreateInfo(
                .{ .compute_bit = true },
                &module_create_info,
                null,
            ),
        );
        errdefer vkd.destroyPipeline(device, pipeline, null);

        var sets: [1]vk.DescriptorSet = undefined;
        try pipeline_module.allocateDescriptorSets(vkd, device, descriptor_pool, &.{descriptor_set_layout}, &sets);

        return .{
            .descriptor_set_layout = descriptor_set_layout,
            .pipeline_layout = pipeline_layout,
            .pipeline = pipeline,
            .descriptor_set = sets[0],
        };
    }

    pub fn deinit(self: *Resources, vkd: anytype, device: vk.Device) void {
        vkd.destroyPipeline(device, self.pipeline, null);
        vkd.destroyPipelineLayout(device, self.pipeline_layout, null);
        vkd.destroyDescriptorSetLayout(device, self.descriptor_set_layout, null);
    }
};

/// What the declaration phase hands to the record phase.
pub const FrameGraphRecord = struct {
    gradient_pass: fg.RenderPassHandle,
    copy_pass: fg.RenderPassHandle,
    present_pass: fg.RenderPassHandle,

    gradient_texture: fg.ResourceUsageHandle,
    copy_src: fg.ResourceUsageHandle,
    copy_dst: fg.ResourceUsageHandle,
    present_src: fg.ResourceUsageHandle,

    extent: vk.Extent2D,
};

pub fn createFrameGraphRecord(builder: *Builder, extent: vk.Extent2D, format: vk.Format) !FrameGraphRecord {
    var record: FrameGraphRecord = undefined;
    record.extent = extent;

    record.gradient_pass = try builder.createRenderPass("DebugGradient", false);
    {
        var properties = gpu_texture_properties.defaultTextureProperties(
            extent.width,
            extent.height,
            format,
            .{ .storage = true, .transfer_src = true },
        );
        properties.misc_flags = .none;

        record.gradient_texture = try builder.createTexture(
            record.gradient_pass,
            "DebugGradientTexture",
            properties,
            access_compute_write,
            &.{},
        );
    }

    record.copy_pass = try builder.createRenderPass("DebugGradientCopy", false);
    {
        record.copy_src = try builder.readTexture(
            record.copy_pass,
            record.gradient_texture,
            access_transfer_src,
            &.{},
        );

        const properties = gpu_texture_properties.defaultTextureProperties(
            extent.width,
            extent.height,
            format,
            .{ .transfer_dst = true, .transfer_src = true },
        );

        record.copy_dst = try builder.createTexture(
            record.copy_pass,
            "DebugGradientCopyTexture",
            properties,
            access_transfer_dst,
            &.{},
        );
    }

    // The only pass with side effects, so the only DAG root: everything above
    // survives pruning because this pass reaches it.
    record.present_pass = try builder.createRenderPass("DebugGradientPresent", true);
    record.present_src = try builder.readTexture(
        record.present_pass,
        record.copy_dst,
        access_blit_src,
        &.{},
    );

    return record;
}

pub fn updateDescriptorSet(
    write_helper: *descriptor_set.DescriptorWriteHelper,
    resources: *const Resources,
    frame_graph_resources: *const FrameGraphResources,
    framegraph: *const fg.FrameGraph,
    record: FrameGraphRecord,
) void {
    const texture = frame_graph_resources.getTexture(framegraph, record.gradient_texture);

    write_helper.appendImage(
        resources.descriptor_set,
        0,
        .storage_image,
        texture.default_view_handle,
        access_compute_write.image_layout,
    );
}

pub fn recordCommandBuffer(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    helper: frame_graph_pass.FrameGraphHelper,
    resources: *const Resources,
    record: FrameGraphRecord,
    swapchain_image: vk.Image,
) void {
    // ---- Gradient ----
    {
        frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.gradient_pass);
        defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.gradient_pass);

        vkd.cmdBindPipeline(cmd_buffer, .compute, resources.pipeline);

        const sets = [_]vk.DescriptorSet{resources.descriptor_set};
        vkd.cmdBindDescriptorSets(cmd_buffer, .compute, resources.pipeline_layout, 0, &sets, &.{});

        const push_constants = hlsl.DebugGradientPushConstants{
            .extent_ts = .{ .x = record.extent.width, .y = record.extent.height },
            .extent_ts_inv = .{
                .x = 1.0 / @as(f32, @floatFromInt(record.extent.width)),
                .y = 1.0 / @as(f32, @floatFromInt(record.extent.height)),
            },
        };

        vkd.cmdPushConstants(
            cmd_buffer,
            resources.pipeline_layout,
            .{ .compute_bit = true },
            0,
            @sizeOf(hlsl.DebugGradientPushConstants),
            &push_constants,
        );

        vkd.cmdDispatch(
            cmd_buffer,
            divRoundUp(record.extent.width, hlsl.DebugGradientThreadCountX),
            divRoundUp(record.extent.height, hlsl.DebugGradientThreadCountY),
            1,
        );
    }

    // ---- Copy A -> B ----
    {
        frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.copy_pass);
        defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.copy_pass);

        const subresource = vk.ImageSubresourceLayers{
            .aspect_mask = .{ .color_bit = true },
            .mip_level = 0,
            .base_array_layer = 0,
            .layer_count = 1,
        };

        const regions = [_]vk.ImageCopy{.{
            .src_subresource = subresource,
            .src_offset = .{ .x = 0, .y = 0, .z = 0 },
            .dst_subresource = subresource,
            .dst_offset = .{ .x = 0, .y = 0, .z = 0 },
            .extent = .{ .width = record.extent.width, .height = record.extent.height, .depth = 1 },
        }};

        vkd.cmdCopyImage(
            cmd_buffer,
            helper.resources.getTexture(helper.frame_graph, record.copy_src).handle,
            access_transfer_src.image_layout,
            helper.resources.getTexture(helper.frame_graph, record.copy_dst).handle,
            access_transfer_dst.image_layout,
            &regions,
        );
    }

    // ---- Blit onto the swapchain ----
    //
    // The swapchain image is not a frame graph resource, so its own barriers
    // are handled by execute_frame; only the graph-owned source is transitioned
    // by the barrier scope here.
    {
        frame_graph_pass.beginBarrierScope(vkd, cmd_buffer, helper, record.present_pass);
        defer frame_graph_pass.endBarrierScope(vkd, cmd_buffer, helper, record.present_pass);

        const subresource = vk.ImageSubresourceLayers{
            .aspect_mask = .{ .color_bit = true },
            .mip_level = 0,
            .base_array_layer = 0,
            .layer_count = 1,
        };

        const regions = [_]vk.ImageBlit{.{
            .src_subresource = subresource,
            .src_offsets = .{
                .{ .x = 0, .y = 0, .z = 0 },
                .{ .x = @intCast(record.extent.width), .y = @intCast(record.extent.height), .z = 1 },
            },
            .dst_subresource = subresource,
            .dst_offsets = .{
                .{ .x = 0, .y = 0, .z = 0 },
                .{ .x = @intCast(record.extent.width), .y = @intCast(record.extent.height), .z = 1 },
            },
        }};

        vkd.cmdBlitImage(
            cmd_buffer,
            helper.resources.getTexture(helper.frame_graph, record.present_src).handle,
            access_blit_src.image_layout,
            swapchain_image,
            .transfer_dst_optimal,
            &regions,
            .nearest,
        );
    }
}

fn divRoundUp(value: u32, divisor: u32) u32 {
    return (value + divisor - 1) / divisor;
}
