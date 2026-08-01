const std = @import("std");
const builtin = @import("builtin");

const clap = @import("clap");
const tracy = @import("tracy.zig");

const execute_frame = @import("renderer/vulkan/execute_frame.zig");
const gpu_scope = @import("renderer/vulkan/gpu_scope.zig");
const game_loop = @import("game_loop.zig");
const window_module = @import("renderer/window/window.zig");
const BackendResources = @import("renderer/vulkan/backend_resources.zig").BackendResources;

const scene_module = @import("renderer/scene.zig");
const vk_types = @import("vulkan");

pub const VulkanBackend = @import("renderer/vulkan/Backend.zig").VulkanBackend;

var debug_allocator: std.heap.DebugAllocator(.{}) = .init;
var gpa_allocator = std.heap.GeneralPurposeAllocator(.{}){};

const default_window_width: u32 = 1280;
const default_window_height: u32 = 720;

pub fn main(init: std.process.Init) !void {
    const gpa, const is_debug = switch (builtin.mode) {
        .Debug, .ReleaseSafe => .{ debug_allocator.allocator(), true },
        .ReleaseFast, .ReleaseSmall => .{ gpa_allocator.allocator(), false },
    };

    defer if (is_debug) {
        _ = debug_allocator.deinit();
    };

    if (tracy.enable_allocation) {
        var gpa_tracy = tracy.tracyAllocator(gpa);
        return main_with_allocator(gpa_tracy.allocator(), init);
    } else {
        return main_with_allocator(gpa, init);
    }
}

/// Debug-only shorthands for --swapchain-format. Only the pairings worth
/// A/B-ing are here; anything else is a startup error rather than a silent
/// fallback, since a typo would otherwise look like the heuristic misbehaving.
fn parseForcedFormat(name: ?[]const u8) ?vk_types.SurfaceFormatKHR {
    const n = name orelse return null;

    const table = .{
        .{ "a2r10g10b10_pq", vk_types.Format.a2r10g10b10_unorm_pack32, vk_types.ColorSpaceKHR.hdr10_st2084_ext },
        .{ "a2b10g10r10_pq", vk_types.Format.a2b10g10r10_unorm_pack32, vk_types.ColorSpaceKHR.hdr10_st2084_ext },
        .{ "rgba16f_pq", vk_types.Format.r16g16b16a16_sfloat, vk_types.ColorSpaceKHR.hdr10_st2084_ext },
        .{ "rgba16_pq", vk_types.Format.r16g16b16a16_unorm, vk_types.ColorSpaceKHR.hdr10_st2084_ext },
        .{ "bgra8_pq", vk_types.Format.b8g8r8a8_unorm, vk_types.ColorSpaceKHR.hdr10_st2084_ext },
        .{ "rgba16f_scrgb", vk_types.Format.r16g16b16a16_sfloat, vk_types.ColorSpaceKHR.bt2020_linear_ext },
        .{ "bgra8_srgb", vk_types.Format.b8g8r8a8_srgb, vk_types.ColorSpaceKHR.srgb_nonlinear_khr },
    };

    inline for (table) |entry| {
        if (std.mem.eql(u8, n, entry[0])) {
            return .{ .format = entry[1], .color_space = entry[2] };
        }
    }

    std.debug.panic("unknown --swapchain-format '{s}'", .{n});
}

pub fn main_with_allocator(allocator: std.mem.Allocator, init: std.process.Init) !void {
    const tr = tracy.trace(@src());
    defer tr.end();

    // First we specify what parameters our program can take.
    // We can use `parseParamsComptime` to parse a string into an array of `Param(Help)`.
    const params = comptime clap.parseParamsComptime(
        \\-h, --help                            Display this help and exit.
        \\    --width <u32>                     Window width in pixels.
        \\    --height <u32>                    Window height in pixels.
        \\    --fullscreen                      Start in fullscreen mode.
        \\    --frame-count <u32>               Exit after presenting this many frames.
        \\    --screenshot <str>                Write the last presented frame to this PNG file.
        \\    --mesh <str>                      OBJ file to draw (placeholder scene until M5).
        \\    --raster-gbuffer                  Rasterize the G-buffer instead of filling it from the vis-buffer.
        \\    --hdr                             Request an HDR10 (PQ) swapchain. Needs the display in HDR mode.
        \\    --swapchain-format <str>          Debug: force a format/colorspace pair, e.g. "a2r10g10b10_pq".
    );

    // Initialize our diagnostics, which can be used for reporting useful errors.
    // This is optional. You can also pass `.{}` to `clap.parse` if you don't
    // care about the extra information `Diagnostics` provides.
    var diag = clap.Diagnostic{};
    var res = clap.parse(clap.Help, &params, clap.parsers.default, init.minimal.args, .{
        .diagnostic = &diag,
        .allocator = allocator,
    }) catch |err| {
        // Report useful error and exit.
        try diag.reportToFile(init.io, .stderr(), err);
        return err;
    };
    defer res.deinit();

    if (res.args.help != 0) {
        return clap.usageToFile(init.io, .stdout(), clap.Help, &params);
    }

    var window = try window_module.Window.init(.{
        .title = "Reaper",
        .width = res.args.width orelse default_window_width,
        .height = res.args.height orelse default_window_height,
        .fullscreen = res.args.fullscreen != 0,
    });
    defer window.deinit();

    // The surface extent is in pixels, which is not the same as the requested
    // window size on a scaled display.
    const pixel_size = window.getSizeInPixels();

    var backend = try VulkanBackend.init(
        allocator,
        &window,
        pixel_size.width,
        pixel_size.height,
        res.args.hdr != 0,
        parseForcedFormat(res.args.@"swapchain-format"),
    );
    defer backend.deinit();

    var resources = try BackendResources.init(
        backend.vkd,
        backend.device,
        .{
            .graphics_queue_family_index = backend.physical_device.graphics_queue_family_index,
            .descriptor_pool = backend.global_descriptor_pool,
            .vma_instance = backend.vma_instance,
            .max_draw_indirect_count = backend.physical_device.properties.limits.max_draw_indirect_count,
            .min_storage_buffer_offset_alignment = backend.physical_device.properties.limits.min_storage_buffer_offset_alignment,
            .io = init.io,
            .physical_device_properties = backend.physical_device.properties,
            .graphics_queue_timestamp_valid_bits = backend.physical_device.graphics_queue_timestamp_valid_bits,
        },
        backend.present_info.swapchain_format,
        allocator,
    );
    // Mirrors renderer_stop(): the GPU has to be done with the last frame
    // before any of its resources are torn down.
    defer {
        _ = backend.vkd.deviceWaitIdle(backend.device) catch {};
        resources.deinit(backend.vkd, backend.device, backend.vma_instance);
    }

    // Mirrors renderer_start(): the font atlas has to be on the GPU before the
    // first GUI pass records anything.
    try execute_frame.uploadImGuiFonts(&backend, &resources);

    // Needs a real submit to read the GPU clock, so it goes after the device is
    // up and before any frame is recorded. Registering the profiler is what
    // switches the scopes in every pass from label-only to label + GPU zone.
    try resources.gpu_profiler.createTracyContext(
        backend.vkd,
        backend.device,
        backend.graphics_queue,
        resources.gfx_cmd_buffer.handle,
        resources.gfx_command_pool,
        backend.physical_device.properties,
    );
    gpu_scope.setProfiler(&resources.gpu_profiler);
    defer gpu_scope.setProfiler(null);

    var scene = try scene_module.createGameScene(
        allocator,
        init.io,
        backend.vkd,
        backend.device,
        &resources.mesh_cache,
        &resources.material_resources,
        backend.vma_instance,
    );
    defer scene.deinit(allocator);

    // Same switch as the Rendering window's checkbox; exposed on the command
    // line so a screenshot gate can capture both producers without a human
    // clicking anything.
    backend.options.use_raster_gbuffer = res.args.@"raster-gbuffer" != 0;

    try game_loop.run(allocator, init.io, &window, &backend, &resources, &scene, .{
        .frame_count = res.args.@"frame-count",
        .screenshot_path = res.args.screenshot,
    });
}
