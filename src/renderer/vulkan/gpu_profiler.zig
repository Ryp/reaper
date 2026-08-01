// Tracy GPU zones, reimplemented over Tracy's C API.
//
// The C++ build gets this from TracyVulkan.hpp via TracyVkContextCalibrated
// (BackendResources.cpp:50). That header is C++ only, but everything it does
// goes through the ___tracy_emit_gpu_* C entry points, so this is a
// transliteration of its VkCtx rather than a binding to it.
//
// The shape, mirroring TracyVulkan.hpp:
//
//   * One VkQueryPool of timestamps, used as a ring.
//   * Zone begin writes a timestamp into the next free slot and tells Tracy the
//     slot id; zone end does the same with the following slot.
//   * Once per frame, collect() reads back whatever has resolved and hands the
//     raw tick counts to Tracy, which converts them using the context's period.
//     Slots are only reset after their results have been read.
//
// DEVIATION: the context is created uncalibrated, where the C++ uses the
// calibrated constructor. Calibration needs a host time domain that matches
// Tracy's own clock, and getting that pairing wrong produces a trace that looks
// plausible and is silently skewed. Zone durations are identical either way —
// only the long-run alignment of the GPU timeline against CPU zones can drift.
// VK_EXT_calibrated_timestamps is already enabled on the device, so adding the
// resync later is self-contained.

const std = @import("std");
const vk = @import("vulkan");
const log = std.log.scoped(.vulkan);

const tracy = @import("../../tracy.zig");

/// Tracy addresses queries with a u16 and the ring has to hold every zone in
/// flight across all frames the CPU can run ahead. Two timestamps per zone,
/// ~30 zones a frame, a few frames deep — 4096 is comfortable and still small
/// (8 bytes of results each, so 64 KB of readback).
const query_count: u32 = 4096;

/// This is the only GPU context, so the id is fixed. Tracy allows 255.
const context_id: u8 = 0;

pub const GpuProfiler = struct {
    query_pool: vk.QueryPool,

    /// Monotonic count of queries handed out; `head % query_count` is the slot.
    /// Never reset, so the u16 Tracy sees wraps in step with the ring.
    head: u64,
    /// Oldest slot whose result has not been read back yet.
    tail: u64,

    /// Set when a collect() found unresolved queries and stopped early, so the
    /// next one retries the same span instead of skipping it.
    pending_count: u32,

    results: [query_count * 2]u64,

    /// False when the device cannot timestamp on the graphics queue, in which
    /// case every entry point below is a no-op.
    supported: bool,

    pub fn init(
        vkd: anytype,
        device: vk.Device,
        physical_device_properties: vk.PhysicalDeviceProperties,
        queue_family_properties: vk.QueueFamilyProperties,
    ) !GpuProfiler {
        // timestampPeriod of 0 means timestamps are unsupported entirely;
        // timestampValidBits of 0 means this particular queue cannot write them.
        const period = physical_device_properties.limits.timestamp_period;
        const supported = tracy.enable and
            period > 0.0 and
            queue_family_properties.timestamp_valid_bits > 0;

        if (!supported) {
            if (tracy.enable) {
                log.warn("GPU profiling disabled: timestamp_period={d} valid_bits={d}", .{
                    period, queue_family_properties.timestamp_valid_bits,
                });
            }
            return .{
                .query_pool = .null_handle,
                .head = 0,
                .tail = 0,
                .pending_count = 0,
                .results = undefined,
                .supported = false,
            };
        }

        const create_info = vk.QueryPoolCreateInfo{
            .s_type = .query_pool_create_info,
            .p_next = null,
            .flags = .{},
            .query_type = .timestamp,
            .query_count = query_count,
            .pipeline_statistics = .{},
        };

        const query_pool = try vkd.createQueryPool(device, &create_info, null);
        errdefer vkd.destroyQueryPool(device, query_pool, null);

        // The whole pool starts unreset, and reading an unreset query is
        // undefined. vkResetQueryPool needs VK_EXT_host_query_reset, which is
        // core in 1.2 and therefore available here.
        vkd.resetQueryPool(device, query_pool, 0, query_count);

        return .{
            .query_pool = query_pool,
            .head = 0,
            .tail = 0,
            .pending_count = 0,
            .results = undefined,
            .supported = true,
        };
    }

    pub fn deinit(self: *GpuProfiler, vkd: anytype, device: vk.Device) void {
        if (!self.supported) return;
        vkd.destroyQueryPool(device, self.query_pool, null);
    }

    /// Anchors Tracy's GPU timeline against the CPU one. Needs a real submit:
    /// the only way to read the GPU clock is to have the GPU write it.
    ///
    /// Mirrors TracyVkContext's constructor — record a timestamp, submit, wait,
    /// read it back, hand it to Tracy with the tick period.
    pub fn createTracyContext(
        self: *GpuProfiler,
        vkd: anytype,
        device: vk.Device,
        queue: vk.Queue,
        cmd_buffer: vk.CommandBuffer,
        command_pool: vk.CommandPool,
        physical_device_properties: vk.PhysicalDeviceProperties,
    ) !void {
        if (!self.supported) return;

        try vkd.resetCommandPool(device, command_pool, .{});

        const begin_info = vk.CommandBufferBeginInfo{
            .s_type = .command_buffer_begin_info,
            .p_next = null,
            .flags = .{ .one_time_submit_bit = true },
            .p_inheritance_info = null,
        };

        try vkd.beginCommandBuffer(cmd_buffer, &begin_info);
        vkd.cmdWriteTimestamp2(cmd_buffer, .{ .all_commands_bit = true }, self.query_pool, 0);
        try vkd.endCommandBuffer(cmd_buffer);

        const cmd_buffer_info = vk.CommandBufferSubmitInfo{
            .s_type = .command_buffer_submit_info,
            .p_next = null,
            .command_buffer = cmd_buffer,
            .device_mask = 0,
        };

        const submit_info = [_]vk.SubmitInfo2{.{
            .s_type = .submit_info_2,
            .p_next = null,
            .flags = .{},
            .wait_semaphore_info_count = 0,
            .p_wait_semaphore_infos = undefined,
            .command_buffer_info_count = 1,
            .p_command_buffer_infos = @ptrCast(&cmd_buffer_info),
            .signal_semaphore_info_count = 0,
            .p_signal_semaphore_infos = undefined,
        }};

        try vkd.queueSubmit2(queue, &submit_info, .null_handle);
        try vkd.queueWaitIdle(queue);

        var gpu_time: [1]u64 = undefined;
        _ = try vkd.getQueryPoolResults(
            device,
            self.query_pool,
            0,
            1,
            @sizeOf(u64),
            &gpu_time,
            @sizeOf(u64),
            .{ .@"64_bit" = true, .wait_bit = true },
        );

        vkd.resetQueryPool(device, self.query_pool, 0, 1);

        tracy.gpuNewContext(
            context_id,
            @bitCast(gpu_time[0]),
            physical_device_properties.limits.timestamp_period,
        );
        tracy.gpuContextName(context_id, "Graphics Queue");

        log.info("GPU profiling ready: timestamp period = {d} ns/tick", .{
            physical_device_properties.limits.timestamp_period,
        });
    }

    /// Opens a GPU zone. `srcloc` comes from tracy.gpuSourceLocation.
    /// Returns the query slot, which the caller hands back to `endZone`.
    pub fn beginZone(self: *GpuProfiler, vkd: anytype, cmd_buffer: vk.CommandBuffer, srcloc: u64) u16 {
        if (!self.supported) return 0;

        const query_id = self.nextQuery();

        // TOP_OF_PIPE would report when the command was *reached*, not when its
        // work began; ALL_COMMANDS brackets the actual execution, matching what
        // TracyVulkan.hpp records.
        vkd.cmdWriteTimestamp2(cmd_buffer, .{ .all_commands_bit = true }, self.query_pool, query_id);
        tracy.gpuZoneBegin(context_id, query_id, srcloc);

        return query_id;
    }

    pub fn endZone(self: *GpuProfiler, vkd: anytype, cmd_buffer: vk.CommandBuffer) void {
        if (!self.supported) return;

        const query_id = self.nextQuery();

        vkd.cmdWriteTimestamp2(cmd_buffer, .{ .all_commands_bit = true }, self.query_pool, query_id);
        tracy.gpuZoneEnd(context_id, query_id);
    }

    fn nextQuery(self: *GpuProfiler) u16 {
        const id = self.head % query_count;
        self.head += 1;
        return @intCast(id);
    }

    /// Reads back whatever has resolved and forwards it to Tracy. Call once per
    /// frame, from the CPU, outside command recording — it must not run while
    /// the queries it reads are still in flight, which is why the caller places
    /// it after the timeline wait for frame N-1.
    pub fn collect(self: *GpuProfiler, vkd: anytype, device: vk.Device) void {
        if (!self.supported) return;
        if (self.tail == self.head) return;

        const wrapped_tail: u32 = @intCast(self.tail % query_count);

        var count: u32 = if (self.pending_count != 0) blk: {
            const c = self.pending_count;
            self.pending_count = 0;
            break :blk c;
        } else @intCast(self.head - self.tail);

        // One readback cannot straddle the end of the ring.
        if (wrapped_tail + count > query_count) {
            count = query_count - wrapped_tail;
        }

        // WITH_AVAILABILITY gives (value, available) pairs, so an unresolved
        // query is visible rather than being waited on — this must never block.
        _ = vkd.getQueryPoolResults(
            device,
            self.query_pool,
            wrapped_tail,
            count,
            @sizeOf(u64) * self.results.len,
            &self.results,
            @sizeOf(u64) * 2,
            .{ .@"64_bit" = true, .with_availability_bit = true },
        ) catch return;

        var resolved: u32 = 0;
        while (resolved < count) : (resolved += 1) {
            // Availability is 0 while the GPU has not written the value yet.
            // Everything after the first unresolved query is also unresolved,
            // so stop and retry the remainder next frame.
            if (self.results[resolved * 2 + 1] == 0) {
                self.pending_count = count - resolved;
                break;
            }

            tracy.gpuTime(
                context_id,
                @intCast(wrapped_tail + resolved),
                @bitCast(self.results[resolved * 2]),
            );
        }

        if (resolved == 0) return;

        vkd.resetQueryPool(device, self.query_pool, wrapped_tail, resolved);
        self.tail += resolved;
    }
};
