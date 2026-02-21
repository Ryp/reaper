const std = @import("std");

const clap = @import("clap");
const tracy = @import("tracy.zig");
const builtin = @import("builtin");

const backend_module = @import("renderer/vulkan/Backend.zig");
pub const VulkanBackend = backend_module.VulkanBackend;
const sdl = backend_module.sdl;

var debug_allocator: std.heap.DebugAllocator(.{}) = .init;
var gpa_allocator = std.heap.GeneralPurposeAllocator(.{}){};

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

pub fn main_with_allocator(allocator: std.mem.Allocator, init: std.process.Init) !void {
    const tr = tracy.trace(@src());
    defer tr.end();

    // First we specify what parameters our program can take.
    // We can use `parseParamsComptime` to parse a string into an array of `Param(Help)`.
    const params = comptime clap.parseParamsComptime(
        \\-h, --help                            Display this help and exit.
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

    if (!sdl.SDL_Init(sdl.SDL_INIT_VIDEO)) {
        std.debug.print("SDL_Init failed: {s}\n", .{sdl.SDL_GetError()});
        return error.SDLInitFailed;
    }
    defer sdl.SDL_Quit();

    const window_width: u32 = 1280;
    const window_height: u32 = 720;

    const window = sdl.SDL_CreateWindow(
        "Reaper",
        @intCast(window_width),
        @intCast(window_height),
        sdl.SDL_WINDOW_VULKAN,
    ) orelse {
        std.debug.print("SDL_CreateWindow failed: {s}\n", .{sdl.SDL_GetError()});
        return error.SDLWindowCreationFailed;
    };
    defer sdl.SDL_DestroyWindow(window);

    var backend = VulkanBackend.init(allocator, window, window_width, window_height) catch |err| {
        std.debug.print("Failed to initialize Vulkan backend: {}\n", .{err});
        return err;
    };
    defer backend.deinit();

    // sleep for 10 seconds (placeholder until the main loop exists)
    sdl.SDL_Delay(10_000);

    // try loop.run(&state, allocator);
}
