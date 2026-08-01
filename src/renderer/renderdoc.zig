// Port of src/renderer/renderdoc/RenderDoc.{h,cpp}
//
// In-application RenderDoc control. Loading librenderdoc.so into the process
// is what makes RenderDoc's own capture keys work and lets the app trigger
// captures itself — no launching through qrenderdoc, which is the point: the
// launcher path fails outright on an HDR Wayland session.
//
// Two things about the load are load-bearing:
//
//   * It must happen BEFORE the Vulkan instance is created. RenderDoc inserts
//     its capture layer at instance creation, so a later dlopen gets a library
//     that is present but hooked into nothing.
//   * The library is never unloaded, even on shutdown. The C++ says the same
//     (RenderDoc.cpp:63) and links the bgfx issue explaining why.
//
// DEVIATION: the C++ hard-codes the .so path at build time through CMake's
// FindRenderDoc (REAPER_RENDERDOC_LIB_NAME) and asserts on every failure. This
// resolves by soname at runtime and degrades to a warning instead, so a build
// without RenderDoc installed still runs — there is no compile-time switch to
// mirror REAPER_USE_RENDERDOC.

const std = @import("std");
const vk = @import("vulkan");
const log = std.log.scoped(.renderdoc);

/// eRENDERDOC_API_Version_1_4_0. The struct below is a prefix of every later
/// version, so asking for 1.4.0 works against a newer library.
const api_version_1_4_0: c_int = 10400;

const library_name = "librenderdoc.so";

/// RENDERDOC_API_1_4_0 from renderdoc_app.h. Field ORDER is the ABI — the
/// entries this file never calls still have to be here, in place, or every
/// pointer after them is wrong. They are typed as opaque to make that explicit.
const Api = extern struct {
    const Fn = *const anyopaque;

    GetAPIVersion: *const fn (*c_int, *c_int, *c_int) callconv(.c) void,
    SetCaptureOptionU32: Fn,
    SetCaptureOptionF32: Fn,
    GetCaptureOptionU32: Fn,
    GetCaptureOptionF32: Fn,
    SetFocusToggleKeys: Fn,
    SetCaptureKeys: Fn,
    GetOverlayBits: Fn,
    MaskOverlayBits: Fn,
    /// Union of Shutdown / RemoveHooks — one pointer either way.
    Shutdown: Fn,
    UnloadCrashHandler: Fn,
    /// Union of SetLogFilePathTemplate / SetCaptureFilePathTemplate.
    SetCaptureFilePathTemplate: *const fn ([*:0]const u8) callconv(.c) void,
    /// Union of GetLogFilePathTemplate / GetCaptureFilePathTemplate.
    GetCaptureFilePathTemplate: Fn,
    GetNumCaptures: *const fn () callconv(.c) u32,
    GetCapture: Fn,
    TriggerCapture: *const fn () callconv(.c) void,
    /// Union of IsRemoteAccessConnected / IsTargetControlConnected.
    IsTargetControlConnected: Fn,
    LaunchReplayUI: Fn,
    SetActiveWindow: Fn,
    StartFrameCapture: *const fn (?*anyopaque, ?*anyopaque) callconv(.c) void,
    IsFrameCapturing: *const fn () callconv(.c) u32,
    EndFrameCapture: *const fn (?*anyopaque, ?*anyopaque) callconv(.c) u32,
    TriggerMultiFrameCapture: *const fn (u32) callconv(.c) void,
};

const GetApiFn = *const fn (c_int, *?*Api) callconv(.c) c_int;

var api: ?*Api = null;

pub fn isAvailable() bool {
    return api != null;
}

/// Mirrors start_integration. Call before creating the Vulkan instance.
/// Returns false when RenderDoc is not installed, which is not an error.
pub fn startIntegration() bool {
    std.debug.assert(api == null);

    var lib = std.DynLib.open(library_name) catch |err| {
        log.warn("could not load {s} ({t}); in-app capture is unavailable", .{ library_name, err });
        return false;
    };
    // Deliberately not closed, here or at shutdown. Unloading RenderDoc after
    // it has hooked the process crashes; see RenderDoc.cpp:63.

    const get_api = lib.lookup(GetApiFn, "RENDERDOC_GetAPI") orelse {
        log.warn("{s} has no RENDERDOC_GetAPI", .{library_name});
        return false;
    };

    var loaded: ?*Api = null;
    if (get_api(api_version_1_4_0, &loaded) != 1 or loaded == null) {
        log.warn("RENDERDOC_GetAPI rejected version {d}", .{api_version_1_4_0});
        return false;
    }

    api = loaded;

    var major: c_int = 0;
    var minor: c_int = 0;
    var patch: c_int = 0;
    loaded.?.GetAPIVersion(&major, &minor, &patch);
    log.info("integration started, API version {d}.{d}.{d}", .{ major, minor, patch });

    return true;
}

/// RenderDoc identifies a Vulkan device by the instance's dispatch table
/// pointer, not by the handle — RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE in
/// renderdoc_app.h:505 is `*(void **)inst`. Passing the handle itself silently
/// captures nothing.
fn devicePointer(instance: vk.Instance) *anyopaque {
    const handle: *const *anyopaque = @ptrFromInt(@intFromEnum(instance));
    return handle.*;
}

pub fn startCapture(instance: vk.Instance) void {
    const a = api orelse return;
    log.info("starting capture", .{});
    a.StartFrameCapture(devicePointer(instance), null);
}

pub fn endCapture(instance: vk.Instance) void {
    const a = api orelse return;

    const ok = a.EndFrameCapture(devicePointer(instance), null);
    if (ok == 1) {
        log.info("capture written ({d} total)", .{a.GetNumCaptures()});
    } else {
        // Not fatal: EndFrameCapture returns 0 when no capture was in flight,
        // which happens if the frame was dropped between start and end.
        log.warn("capture discarded", .{});
    }
}

/// Queues a capture of the next presented frame. Unlike start/endCapture this
/// needs no instance, and it is what RenderDoc's own F12 key does.
pub fn triggerCapture() void {
    const a = api orelse return;
    log.info("queued a capture of the next frame", .{});
    a.TriggerCapture();
}

/// Where captures land. RenderDoc appends a frame number and `.rdc`.
pub fn setCapturePathTemplate(path: [:0]const u8) void {
    const a = api orelse return;
    a.SetCaptureFilePathTemplate(path.ptr);
}
