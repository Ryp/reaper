// Port of src/renderer/window/Window.h + Event.h, backed by SDL3.
//
// The renderer only ever sees the types declared here, so a native Wayland
// backend can replace the SDL implementation later without touching any
// renderer code (same split as IWindow / WaylandWindow on the C++ side).

const std = @import("std");
const sdl = @import("sdl.zig").c;
const log = std.log.scoped(.window);

// --------------------------------------------------------------------------
// Event types (Event.h)
// --------------------------------------------------------------------------

pub const MouseButton = enum {
    invalid,
    left,
    middle,
    right,
};

pub const KeyCode = enum {
    invalid,
    num_1,
    num_2,
    num_3,
    num_4,
    num_5,
    num_6,
    num_7,
    num_8,
    num_9,
    num_0,
    escape,
    enter,
    space,
    arrow_right,
    arrow_left,
    arrow_down,
    arrow_up,
    w,
    a,
    s,
    d,
};

/// C++ uses a type tag plus an untagged union; Zig models the same thing as a
/// tagged union, which is why there is no `Invalid` variant here.
pub const Event = union(enum) {
    resize: Resize,
    mouse_button: MouseButtonPress,
    mouse_wheel: MouseWheel,
    key_press: KeyPress,
    close,

    pub const Resize = struct {
        width: u32,
        height: u32,
    };

    pub const MouseButtonPress = struct {
        button: MouseButton,
        press: bool,
    };

    pub const MouseWheel = struct {
        x_delta: i32,
        y_delta: i32,
    };

    pub const KeyPress = struct {
        key: KeyCode,
        press: bool,
        /// Platform scancode, kept for logging parity with the C++ backends.
        internal_key_code: u32,
    };
};

// --------------------------------------------------------------------------
// Window (Window.h)
// --------------------------------------------------------------------------

pub const MouseState = struct {
    pos_x: i32,
    pos_y: i32,
};

pub const CreationDescriptor = struct {
    title: [:0]const u8,
    width: u32,
    height: u32,
    fullscreen: bool = false,
};

pub const Window = struct {
    handle: *sdl.SDL_Window,

    /// Initializes the SDL video subsystem and opens a Vulkan-capable window.
    pub fn init(desc: CreationDescriptor) !Window {
        // Prefer Wayland, which is what the C++ window backend uses. This is
        // not cosmetic: the WSI a surface is created through decides which
        // formats it reports, and on RADV the X11 one offers two where the
        // Wayland one offers eighteen — so the two builds would otherwise pick
        // different swapchain formats and render differently. X11 stays as a
        // fallback, and SDL_VIDEO_DRIVER in the environment still wins over
        // this, since SDL_SetHint does not override an explicit user setting.
        _ = sdl.SDL_SetHint(sdl.SDL_HINT_VIDEO_DRIVER, "wayland,x11");

        if (!sdl.SDL_Init(sdl.SDL_INIT_VIDEO)) {
            log.err("SDL_Init failed: {s}", .{sdl.SDL_GetError()});
            return error.WindowInitFailed;
        }
        errdefer sdl.SDL_Quit();

        // HIGH_PIXEL_DENSITY asks for a backbuffer at the display's real pixel
        // size rather than at the scaled logical size. Without it, a window on
        // a scaled output gets a logical-sized swapchain that the compositor
        // then upscales, and the whole frame is soft. The rest of the code is
        // already in pixel space — getSizeInPixels() drives the swapchain and
        // the resize path listens for PIXEL_SIZE_CHANGED — so this flag is the
        // only thing that was missing. It is a no-op at scale 1.
        var flags: sdl.SDL_WindowFlags = sdl.SDL_WINDOW_VULKAN |
            sdl.SDL_WINDOW_RESIZABLE |
            sdl.SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (desc.fullscreen) {
            flags |= sdl.SDL_WINDOW_FULLSCREEN;
        }

        const handle = sdl.SDL_CreateWindow(
            desc.title.ptr,
            @intCast(desc.width),
            @intCast(desc.height),
            flags,
        ) orelse {
            log.err("SDL_CreateWindow failed: {s}", .{sdl.SDL_GetError()});
            return error.WindowCreationFailed;
        };

        return .{ .handle = handle };
    }

    pub fn deinit(self: *Window) void {
        sdl.SDL_DestroyWindow(self.handle);
        sdl.SDL_Quit();
    }

    /// Drains the SDL queue into `events`, translating to our own event type.
    /// Unrecognized SDL events are dropped.
    pub fn pumpEvents(self: *Window, allocator: std.mem.Allocator, events: *std.ArrayList(Event)) !void {
        var sdl_event: sdl.SDL_Event = undefined;

        while (sdl.SDL_PollEvent(&sdl_event)) {
            // Ignore anything addressed at another window.
            const translated = self.translateEvent(&sdl_event) orelse continue;
            try events.append(allocator, translated);
        }
    }

    fn translateEvent(self: *Window, sdl_event: *const sdl.SDL_Event) ?Event {
        const window_id = sdl.SDL_GetWindowID(self.handle);

        return switch (sdl_event.type) {
            sdl.SDL_EVENT_QUIT => .close,
            sdl.SDL_EVENT_WINDOW_CLOSE_REQUESTED => blk: {
                if (sdl_event.window.windowID != window_id) break :blk null;
                break :blk .close;
            },
            // The swapchain is sized in pixels, so this is the event that
            // matters on HiDPI displays, not SDL_EVENT_WINDOW_RESIZED.
            sdl.SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED => blk: {
                if (sdl_event.window.windowID != window_id) break :blk null;
                if (sdl_event.window.data1 <= 0 or sdl_event.window.data2 <= 0) break :blk null;
                break :blk .{ .resize = .{
                    .width = @intCast(sdl_event.window.data1),
                    .height = @intCast(sdl_event.window.data2),
                } };
            },
            sdl.SDL_EVENT_MOUSE_BUTTON_DOWN, sdl.SDL_EVENT_MOUSE_BUTTON_UP => .{ .mouse_button = .{
                .button = translateMouseButton(sdl_event.button.button),
                .press = sdl_event.button.down,
            } },
            sdl.SDL_EVENT_MOUSE_WHEEL => .{ .mouse_wheel = .{
                .x_delta = sdl_event.wheel.integer_x,
                .y_delta = sdl_event.wheel.integer_y,
            } },
            sdl.SDL_EVENT_KEY_DOWN, sdl.SDL_EVENT_KEY_UP => blk: {
                // Key auto-repeat would ruin the WASD axis handling, same
                // caveat as the C++ loop, so drop repeats outright.
                if (sdl_event.key.repeat) break :blk null;
                break :blk .{ .key_press = .{
                    .key = translateKeyCode(sdl_event.key.scancode),
                    .press = sdl_event.key.down,
                    .internal_key_code = @intCast(sdl_event.key.scancode),
                } };
            },
            else => null,
        };
    }

    /// Mouse position in PIXELS, to match the swapchain and the ImGui display
    /// size. SDL reports it in logical window coordinates, which differ from
    /// pixels by exactly the pixel density once HIGH_PIXEL_DENSITY is on —
    /// leaving it unscaled puts the cursor at half its true position on a 2x
    /// display, so every ImGui hit test lands in the wrong place.
    pub fn getMouseState(self: *Window) MouseState {
        var x: f32 = 0;
        var y: f32 = 0;
        _ = sdl.SDL_GetMouseState(&x, &y);

        const density = self.getPixelDensity();

        return .{
            .pos_x = @intFromFloat(x * density),
            .pos_y = @intFromFloat(y * density),
        };
    }

    /// Pixel size divided by logical size: 1.0 unscaled, 2.0 on a 200% display.
    /// SDL returns 0 on error, which would silently collapse the UI, so the
    /// fallback is an unscaled 1.0.
    pub fn getPixelDensity(self: *Window) f32 {
        const density = sdl.SDL_GetWindowPixelDensity(self.handle);
        return if (density > 0.0) density else 1.0;
    }

    /// Pixel size of the drawable area — this is what the swapchain wants.
    pub fn getSizeInPixels(self: *Window) struct { width: u32, height: u32 } {
        var w: c_int = 0;
        var h: c_int = 0;
        if (!sdl.SDL_GetWindowSizeInPixels(self.handle, &w, &h)) {
            log.err("SDL_GetWindowSizeInPixels failed: {s}", .{sdl.SDL_GetError()});
            return .{ .width = 0, .height = 0 };
        }
        return .{ .width = @intCast(@max(w, 0)), .height = @intCast(@max(h, 0)) };
    }

    /// True while the window is minimized; the swapchain extent is degenerate
    /// then and the frame must be skipped.
    pub fn isMinimized(self: *Window) bool {
        return (sdl.SDL_GetWindowFlags(self.handle) & sdl.SDL_WINDOW_MINIMIZED) != 0;
    }

    /// Whether this window's output is actually in HDR mode.
    ///
    /// Nothing in Vulkan answers this. A surface's VkColorSpaceKHR says which
    /// colour space a swapchain would be interpreted in, not which one the
    /// display is currently configured for — the compositor offers HDR10 as
    /// soon as it can *convert* it, whether or not HDR is switched on.
    ///
    /// SDL fills this in from wp_color_manager_v1's luminance range
    /// (SDL_waylandcolor.c:174), so it does track the compositor. The
    /// per-window property is used rather than the per-display one because it
    /// is the window's own output that matters when they differ.
    pub fn isHdrEnabled(self: *Window) bool {
        // The DISPLAY property, not the window one. The window property
        // reports what SDL has configured that window's surface for, and since
        // the swapchain is driven through Vulkan directly SDL is never told we
        // want HDR — it reads false with headroom 1.0 even on an output that
        // is in HDR mode. Verified side by side: window has=true val=false,
        // display has=true val=true, with `output DP-1 hdr on`.
        //
        // SDL fills the display property from wp_color_manager_v1's image
        // description for the output (SDL_waylandcolor.c:174), which is
        // exactly the question being asked here.
        const display = sdl.SDL_GetDisplayForWindow(self.handle);
        if (display == 0) return false;

        return sdl.SDL_GetBooleanProperty(
            sdl.SDL_GetDisplayProperties(display),
            sdl.SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN,
            false,
        );
    }

    /// Reported HDR headroom for this window, or 1.0 when there is none.
    ///
    /// NOTE: this is window-scoped and shares the caveat above — SDL clamps it
    /// to 1.0 for a window it has not put into an HDR colour space, which is
    /// every window here. Kept because it is the shape the real number will
    /// take once the luminance range is read from wp_color_manager_v1
    /// directly, but it is not usable as a display-capability signal today.
    pub fn getHdrHeadroom(self: *Window) f32 {
        return sdl.SDL_GetFloatProperty(
            sdl.SDL_GetWindowProperties(self.handle),
            sdl.SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT,
            1.0,
        );
    }

    /// The video backend SDL actually selected, for logging: which WSI a
    /// surface goes through decides the swapchain formats on offer.
    pub fn getVideoDriverName(_: *Window) []const u8 {
        const name = sdl.SDL_GetCurrentVideoDriver() orelse return "unknown";
        return std.mem.span(name);
    }
};

// --------------------------------------------------------------------------
// Translation tables
// --------------------------------------------------------------------------

fn translateMouseButton(button: u8) MouseButton {
    return switch (button) {
        sdl.SDL_BUTTON_LEFT => .left,
        sdl.SDL_BUTTON_MIDDLE => .middle,
        sdl.SDL_BUTTON_RIGHT => .right,
        else => .invalid,
    };
}

/// Scancodes (physical key positions) rather than keycodes, to match what the
/// XCB/Wayland backends map on the C++ side.
fn translateKeyCode(scancode: sdl.SDL_Scancode) KeyCode {
    return switch (scancode) {
        sdl.SDL_SCANCODE_1 => .num_1,
        sdl.SDL_SCANCODE_2 => .num_2,
        sdl.SDL_SCANCODE_3 => .num_3,
        sdl.SDL_SCANCODE_4 => .num_4,
        sdl.SDL_SCANCODE_5 => .num_5,
        sdl.SDL_SCANCODE_6 => .num_6,
        sdl.SDL_SCANCODE_7 => .num_7,
        sdl.SDL_SCANCODE_8 => .num_8,
        sdl.SDL_SCANCODE_9 => .num_9,
        sdl.SDL_SCANCODE_0 => .num_0,
        sdl.SDL_SCANCODE_ESCAPE => .escape,
        sdl.SDL_SCANCODE_RETURN => .enter,
        sdl.SDL_SCANCODE_SPACE => .space,
        sdl.SDL_SCANCODE_RIGHT => .arrow_right,
        sdl.SDL_SCANCODE_LEFT => .arrow_left,
        sdl.SDL_SCANCODE_DOWN => .arrow_down,
        sdl.SDL_SCANCODE_UP => .arrow_up,
        sdl.SDL_SCANCODE_W => .w,
        sdl.SDL_SCANCODE_A => .a,
        sdl.SDL_SCANCODE_S => .s,
        sdl.SDL_SCANCODE_D => .d,
        else => .invalid,
    };
}

/// Sleep, without pulling SDL into the caller.
pub fn delay(milliseconds: u32) void {
    sdl.SDL_Delay(milliseconds);
}

pub fn getMouseButtonString(button: MouseButton) []const u8 {
    return @tagName(button);
}

pub fn getKeyboardKeyString(key: KeyCode) []const u8 {
    return @tagName(key);
}
