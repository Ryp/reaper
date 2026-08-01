// Port of src/renderer/vulkan/GpuProfile.h
//
// The C++ REAPER_GPU_SCOPE macro opens three things at once — a Vulkan debug
// label, a Tracy GPU zone and a Tracy CPU zone — and closes them all when the
// enclosing scope ends. Zig has no destructors, so instead of a macro this is a
// value with an `end()` that callers pair with `defer`:
//
//     const scope = gpu_scope.begin(vkd, cmd_buffer, @src(), "Forward");
//     defer scope.end(vkd, cmd_buffer);
//
// `@src()` is what the macro's __LINE__ token-pasting was standing in for: it
// gives Tracy the file and line for free, which the C++ had to get from the
// ZoneScopedN expansion.

const std = @import("std");
const vk = @import("vulkan");

const debug_label = @import("debug_label.zig");
const tracy = @import("../../tracy.zig");

const GpuProfiler = @import("gpu_profiler.zig").GpuProfiler;

/// The one profiler the scopes write into. A parameter would have to be
/// threaded through every pass's record signature — 28 call sites across 14
/// files — purely to reach a single process-wide object, which is what the C++
/// macro avoided by closing over `command_buffer.tracy_ctx`.
var profiler: ?*GpuProfiler = null;

pub fn setProfiler(p: ?*GpuProfiler) void {
    profiler = p;
}

pub const Scope = struct {
    cpu_zone: tracy.Ctx,

    /// Closes the GPU zone, the label and the CPU zone, in the reverse order
    /// they opened.
    pub fn end(self: Scope, vkd: anytype, cmd_buffer: vk.CommandBuffer) void {
        if (profiler) |p| p.endZone(vkd, cmd_buffer);
        debug_label.end(vkd, cmd_buffer);
        self.cpu_zone.end();
    }
};

/// `name` must be a comptime string: Tracy keeps the pointer for the lifetime
/// of the program rather than copying, and the label wants a sentinel.
pub fn begin(
    vkd: anytype,
    cmd_buffer: vk.CommandBuffer,
    comptime src: std.builtin.SourceLocation,
    comptime name: [:0]const u8,
) Scope {
    debug_label.begin(vkd, cmd_buffer, name.ptr);

    if (profiler) |p| {
        _ = p.beginZone(vkd, cmd_buffer, tracy.gpuSourceLocation(src, name));
    }

    return .{ .cpu_zone = tracy.traceNamed(src, name) };
}
