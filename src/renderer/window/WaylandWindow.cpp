////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "WaylandWindow.h"

#include "Event.h"

#include <core/Assert.h>

#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

namespace Reaper
{
namespace
{
    // ── Key / button conversion ───────────────────────────────────────────────

    Window::KeyCode::type convert_xkb_keysym(xkb_keysym_t sym)
    {
        switch (sym)
        {
        case XKB_KEY_Escape:
            return Window::KeyCode::ESCAPE;
        case XKB_KEY_Return:
            return Window::KeyCode::ENTER;
        case XKB_KEY_space:
            return Window::KeyCode::SPACE;
        case XKB_KEY_Right:
            return Window::KeyCode::ARROW_RIGHT;
        case XKB_KEY_Left:
            return Window::KeyCode::ARROW_LEFT;
        case XKB_KEY_Down:
            return Window::KeyCode::ARROW_DOWN;
        case XKB_KEY_Up:
            return Window::KeyCode::ARROW_UP;
        case XKB_KEY_w:
            return Window::KeyCode::W;
        case XKB_KEY_a:
            return Window::KeyCode::A;
        case XKB_KEY_s:
            return Window::KeyCode::S;
        case XKB_KEY_d:
            return Window::KeyCode::D;
        case XKB_KEY_1:
            return Window::KeyCode::NUM_1;
        case XKB_KEY_2:
            return Window::KeyCode::NUM_2;
        case XKB_KEY_3:
            return Window::KeyCode::NUM_3;
        case XKB_KEY_4:
            return Window::KeyCode::NUM_4;
        case XKB_KEY_5:
            return Window::KeyCode::NUM_5;
        case XKB_KEY_6:
            return Window::KeyCode::NUM_6;
        case XKB_KEY_7:
            return Window::KeyCode::NUM_7;
        case XKB_KEY_8:
            return Window::KeyCode::NUM_8;
        case XKB_KEY_9:
            return Window::KeyCode::NUM_9;
        case XKB_KEY_0:
            return Window::KeyCode::NUM_0;
        default:
            return Window::KeyCode::Invalid;
        }
    }

    Window::MouseButton::type convert_linux_button(uint32_t button)
    {
        switch (button)
        {
        case BTN_LEFT:
            return Window::MouseButton::Left;
        case BTN_MIDDLE:
            return Window::MouseButton::Middle;
        case BTN_RIGHT:
            return Window::MouseButton::Right;
        default:
            return Window::MouseButton::Invalid;
        }
    }

    // ── Registry callbacks ────────────────────────────────────────────────────

    void on_registry_global(void* data, wl_registry* registry, uint32_t name, const char* interface,
                            uint32_t /*version*/);
    void on_registry_global_remove(void* data, wl_registry* registry, uint32_t name);

    // ── XDG WM Base callbacks ─────────────────────────────────────────────────

    void on_xdg_wm_base_ping(void* data, xdg_wm_base* base, uint32_t serial);

    // ── XDG Surface callbacks ─────────────────────────────────────────────────

    void on_xdg_surface_configure(void* data, xdg_surface* xdg_surface, uint32_t serial);

    // ── XDG Toplevel callbacks ────────────────────────────────────────────────

    void on_xdg_toplevel_configure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states);
    void on_xdg_toplevel_close(void* data, xdg_toplevel* toplevel);
    void on_xdg_toplevel_configure_bounds(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height);
    void on_xdg_toplevel_wm_capabilities(void* data, xdg_toplevel* toplevel, wl_array* capabilities);

    // ── Seat callbacks ────────────────────────────────────────────────────────

    void on_seat_capabilities(void* data, wl_seat* seat, uint32_t caps);
    void on_seat_name(void* data, wl_seat* seat, const char* name);

    // ── Pointer callbacks ─────────────────────────────────────────────────────

    void on_pointer_enter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t sx,
                          wl_fixed_t sy);
    void on_pointer_leave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface);
    void on_pointer_motion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
    void on_pointer_button(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button,
                           uint32_t state);
    void on_pointer_axis(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
    void on_pointer_frame(void* data, wl_pointer* pointer);
    void on_pointer_axis_source(void* data, wl_pointer* pointer, uint32_t axis_source);
    void on_pointer_axis_stop(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis);
    void on_pointer_axis_discrete(void* data, wl_pointer* pointer, uint32_t axis, int32_t discrete);
    void on_pointer_axis_value120(void* data, wl_pointer* pointer, uint32_t axis, int32_t value120);
    void on_pointer_axis_relative_direction(void* data, wl_pointer* pointer, uint32_t axis, uint32_t direction);

    // ── Keyboard callbacks ────────────────────────────────────────────────────

    void on_keyboard_keymap(void* data, wl_keyboard* keyboard, uint32_t format, int fd, uint32_t size);
    void on_keyboard_enter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys);
    void on_keyboard_leave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface);
    void on_keyboard_key(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state);
    void on_keyboard_modifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
    void on_keyboard_repeat_info(void* data, wl_keyboard* keyboard, int32_t rate, int32_t delay);

    // ── Listener tables ───────────────────────────────────────────────────────
    // Defined after the forward declarations so the handler bodies can reference
    // sibling listeners freely.

    const wl_registry_listener s_registry_listener = {
        .global = on_registry_global,
        .global_remove = on_registry_global_remove,
    };

    const xdg_wm_base_listener s_xdg_wm_base_listener = {
        .ping = on_xdg_wm_base_ping,
    };

    const xdg_surface_listener s_xdg_surface_listener = {
        .configure = on_xdg_surface_configure,
    };

    const xdg_toplevel_listener s_xdg_toplevel_listener = {
        .configure = on_xdg_toplevel_configure,
        .close = on_xdg_toplevel_close,
        .configure_bounds = on_xdg_toplevel_configure_bounds,
        .wm_capabilities = on_xdg_toplevel_wm_capabilities,
    };

    const wl_seat_listener s_seat_listener = {
        .capabilities = on_seat_capabilities,
        .name = on_seat_name,
    };

    const wl_pointer_listener s_pointer_listener = {
        .enter = on_pointer_enter,
        .leave = on_pointer_leave,
        .motion = on_pointer_motion,
        .button = on_pointer_button,
        .axis = on_pointer_axis,
        .frame = on_pointer_frame,
        .axis_source = on_pointer_axis_source,
        .axis_stop = on_pointer_axis_stop,
        .axis_discrete = on_pointer_axis_discrete,
        .axis_value120 = on_pointer_axis_value120,
        .axis_relative_direction = on_pointer_axis_relative_direction,
    };

    const wl_keyboard_listener s_keyboard_listener = {
        .keymap = on_keyboard_keymap,
        .enter = on_keyboard_enter,
        .leave = on_keyboard_leave,
        .key = on_keyboard_key,
        .modifiers = on_keyboard_modifiers,
        .repeat_info = on_keyboard_repeat_info,
    };

    // ── Registry implementation ───────────────────────────────────────────────

    void on_registry_global(void* data, wl_registry* registry, uint32_t name, const char* interface,
                            uint32_t /*version*/)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);

        if (std::strcmp(interface, wl_compositor_interface.name) == 0)
        {
            self->m_compositor =
                static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 4));
        }
        else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0)
        {
            self->m_xdg_wm_base =
                static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
            xdg_wm_base_add_listener(self->m_xdg_wm_base, &s_xdg_wm_base_listener, self);
        }
        else if (std::strcmp(interface, wl_seat_interface.name) == 0)
        {
            self->m_seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 5));
            wl_seat_add_listener(self->m_seat, &s_seat_listener, self);
        }
    }

    void on_registry_global_remove(void* /*data*/, wl_registry* /*registry*/, uint32_t /*name*/)
    {}

    // ── XDG WM Base implementation ────────────────────────────────────────────

    void on_xdg_wm_base_ping(void* /*data*/, xdg_wm_base* base, uint32_t serial)
    {
        xdg_wm_base_pong(base, serial);
    }

    // ── XDG Surface implementation ────────────────────────────────────────────

    void on_xdg_surface_configure(void* data, xdg_surface* xdg_surface, uint32_t serial)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        xdg_surface_ack_configure(xdg_surface, serial);
        self->m_configured = true;
    }

    // ── XDG Toplevel implementation ───────────────────────────────────────────

    void on_xdg_toplevel_configure(void* data, xdg_toplevel* /*toplevel*/, int32_t width, int32_t height,
                                   wl_array* /*states*/)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        // width/height == 0 means "compositor doesn't care; use your preferred size"
        if (width > 0 && height > 0 && self->m_event_output != nullptr)
        {
            self->m_event_output->emplace_back(
                Window::createResizeEvent(static_cast<u32>(width), static_cast<u32>(height)));
        }
    }

    void on_xdg_toplevel_close(void* data, xdg_toplevel* /*toplevel*/)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        if (self->m_event_output != nullptr)
        {
            Window::Event event = {};
            event.type = Window::EventType::Close;
            self->m_event_output->emplace_back(event);
        }
    }

    void on_xdg_toplevel_configure_bounds(void* /*data*/, xdg_toplevel* /*toplevel*/, int32_t /*width*/,
                                          int32_t /*height*/)
    {}

    void on_xdg_toplevel_wm_capabilities(void* /*data*/, xdg_toplevel* /*toplevel*/, wl_array* /*capabilities*/)
    {}

    // ── Seat implementation ───────────────────────────────────────────────────

    void on_seat_capabilities(void* data, wl_seat* seat, uint32_t caps)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);

        if ((caps & WL_SEAT_CAPABILITY_POINTER) && self->m_pointer == nullptr)
        {
            self->m_pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(self->m_pointer, &s_pointer_listener, self);
        }
        else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && self->m_pointer != nullptr)
        {
            wl_pointer_release(self->m_pointer);
            self->m_pointer = nullptr;
        }

        if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && self->m_keyboard == nullptr)
        {
            self->m_keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(self->m_keyboard, &s_keyboard_listener, self);
        }
        else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && self->m_keyboard != nullptr)
        {
            wl_keyboard_release(self->m_keyboard);
            self->m_keyboard = nullptr;
        }
    }

    void on_seat_name(void* /*data*/, wl_seat* /*seat*/, const char* /*name*/)
    {}

    // ── Pointer implementation ────────────────────────────────────────────────

    void on_pointer_enter(void*      data, wl_pointer* /*pointer*/, uint32_t /*serial*/, wl_surface* /*surface*/,
                          wl_fixed_t sx, wl_fixed_t sy)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        self->m_mouse_x = wl_fixed_to_int(sx);
        self->m_mouse_y = wl_fixed_to_int(sy);
    }

    void on_pointer_leave(void* /*data*/, wl_pointer* /*pointer*/, uint32_t /*serial*/, wl_surface* /*surface*/)
    {}

    void on_pointer_motion(void* data, wl_pointer* /*pointer*/, uint32_t /*time*/, wl_fixed_t sx, wl_fixed_t sy)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        self->m_mouse_x = wl_fixed_to_int(sx);
        self->m_mouse_y = wl_fixed_to_int(sy);
    }

    void on_pointer_button(void* data, wl_pointer* /*pointer*/, uint32_t /*serial*/, uint32_t /*time*/, uint32_t button,
                           uint32_t state)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        if (self->m_event_output == nullptr)
            return;

        Window::MouseButton::type btn = convert_linux_button(button);
        if (btn == Window::MouseButton::Invalid)
            return;

        bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
        self->m_event_output->emplace_back(Window::createButtonEvent(btn, pressed));
    }

    void on_pointer_axis(void* data, wl_pointer* /*pointer*/, uint32_t /*time*/, uint32_t axis, wl_fixed_t value)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        if (self->m_event_output == nullptr)
            return;

        i32 delta = wl_fixed_to_int(value);
        if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
            self->m_event_output->emplace_back(Window::createMouseWheelEvent(0, -delta));
        else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
            self->m_event_output->emplace_back(Window::createMouseWheelEvent(delta, 0));
    }

    void on_pointer_frame(void* /*data*/, wl_pointer* /*pointer*/)
    {}
    void on_pointer_axis_source(void* /*data*/, wl_pointer* /*pointer*/, uint32_t /*axis_source*/)
    {}
    void on_pointer_axis_stop(void* /*data*/, wl_pointer* /*pointer*/, uint32_t /*time*/, uint32_t /*axis*/)
    {}
    void on_pointer_axis_discrete(void* /*data*/, wl_pointer* /*pointer*/, uint32_t /*axis*/, int32_t /*discrete*/)
    {}
    void on_pointer_axis_value120(void* /*data*/, wl_pointer* /*pointer*/, uint32_t /*axis*/, int32_t /*value120*/)
    {}
    void on_pointer_axis_relative_direction(void* /*data*/, wl_pointer* /*pointer*/, uint32_t /*axis*/,
                                            uint32_t /*direction*/)
    {}

    // ── Keyboard implementation ───────────────────────────────────────────────

    void on_keyboard_keymap(void* data, wl_keyboard* /*keyboard*/, uint32_t format, int fd, uint32_t size)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);

        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
        {
            close(fd);
            return;
        }

        char* map_str = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        Assert(map_str != MAP_FAILED);

        xkb_keymap* keymap = xkb_keymap_new_from_string(self->m_xkb_context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                        XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map_str, size);
        close(fd);
        Assert(keymap != nullptr);

        xkb_state* state = xkb_state_new(keymap);
        Assert(state != nullptr);

        xkb_keymap_unref(self->m_xkb_keymap);
        xkb_state_unref(self->m_xkb_state);
        self->m_xkb_keymap = keymap;
        self->m_xkb_state = state;
    }

    void on_keyboard_enter(void* /*data*/, wl_keyboard* /*keyboard*/, uint32_t /*serial*/, wl_surface* /*surface*/,
                           wl_array* /*keys*/)
    {}

    void on_keyboard_leave(void* /*data*/, wl_keyboard* /*keyboard*/, uint32_t /*serial*/, wl_surface* /*surface*/)
    {}

    void on_keyboard_key(void* data, wl_keyboard* /*keyboard*/, uint32_t /*serial*/, uint32_t /*time*/, uint32_t key,
                         uint32_t state)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        if (self->m_event_output == nullptr || self->m_xkb_state == nullptr)
            return;

        // Wayland sends evdev keycodes; xkbcommon expects evdev + 8
        xkb_keycode_t         keycode = key + 8;
        xkb_keysym_t          keysym = xkb_state_key_get_one_sym(self->m_xkb_state, keycode);
        Window::KeyCode::type kc = convert_xkb_keysym(keysym);
        bool                  pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
        self->m_event_output->emplace_back(Window::createKeyEvent(kc, pressed, key));
    }

    void on_keyboard_modifiers(void* data, wl_keyboard* /*keyboard*/, uint32_t /*serial*/, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
    {
        WaylandWindow* self = static_cast<WaylandWindow*>(data);
        if (self->m_xkb_state == nullptr)
            return;
        xkb_state_update_mask(self->m_xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    }

    void on_keyboard_repeat_info(void* /*data*/, wl_keyboard* /*keyboard*/, int32_t /*rate*/, int32_t /*delay*/)
    {}

} // namespace

// ── Constructor / Destructor ──────────────────────────────────────────────────

WaylandWindow::WaylandWindow(const WindowCreationDescriptor& creationInfo)
    : m_display(nullptr)
    , m_surface(nullptr)
    , m_registry(nullptr)
    , m_compositor(nullptr)
    , m_xdg_wm_base(nullptr)
    , m_seat(nullptr)
    , m_xdg_surface(nullptr)
    , m_xdg_toplevel(nullptr)
    , m_pointer(nullptr)
    , m_keyboard(nullptr)
    , m_xkb_context(nullptr)
    , m_xkb_keymap(nullptr)
    , m_xkb_state(nullptr)
    , m_mouse_x(0)
    , m_mouse_y(0)
    , m_configured(false)
    , m_event_output(nullptr)
{
    m_display = wl_display_connect(nullptr);
    Assert(m_display != nullptr);

    m_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    Assert(m_xkb_context != nullptr);

    m_registry = wl_display_get_registry(m_display);
    Assert(m_registry != nullptr);
    wl_registry_add_listener(m_registry, &s_registry_listener, this);

    // First roundtrip: receive the list of globals
    wl_display_roundtrip(m_display);
    // Second roundtrip: process events triggered by binding (e.g. seat capabilities)
    wl_display_roundtrip(m_display);

    Assert(m_compositor != nullptr);
    Assert(m_xdg_wm_base != nullptr);

    m_surface = wl_compositor_create_surface(m_compositor);
    Assert(m_surface != nullptr);

    m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface);
    Assert(m_xdg_surface != nullptr);
    xdg_surface_add_listener(m_xdg_surface, &s_xdg_surface_listener, this);

    m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
    Assert(m_xdg_toplevel != nullptr);
    xdg_toplevel_add_listener(m_xdg_toplevel, &s_xdg_toplevel_listener, this);

    xdg_toplevel_set_title(m_xdg_toplevel, creationInfo.title);
    xdg_toplevel_set_app_id(m_xdg_toplevel, "reaper");
}

WaylandWindow::~WaylandWindow()
{
    if (m_xkb_state)
        xkb_state_unref(m_xkb_state);
    if (m_xkb_keymap)
        xkb_keymap_unref(m_xkb_keymap);
    if (m_xkb_context)
        xkb_context_unref(m_xkb_context);

    if (m_keyboard)
        wl_keyboard_release(m_keyboard);
    if (m_pointer)
        wl_pointer_release(m_pointer);
    if (m_seat)
        wl_seat_release(m_seat);

    if (m_xdg_toplevel)
        xdg_toplevel_destroy(m_xdg_toplevel);
    if (m_xdg_surface)
        xdg_surface_destroy(m_xdg_surface);
    if (m_surface)
        wl_surface_destroy(m_surface);

    if (m_xdg_wm_base)
        xdg_wm_base_destroy(m_xdg_wm_base);
    if (m_compositor)
        wl_compositor_destroy(m_compositor);
    if (m_registry)
        wl_registry_destroy(m_registry);

    wl_display_disconnect(m_display);
}

// ── IWindow implementation ────────────────────────────────────────────────────

void WaylandWindow::map()
{
    // Commit the initial surface state to signal readiness to the compositor
    wl_surface_commit(m_surface);
    wl_display_flush(m_display);

    // Block until the compositor sends the first configure event
    while (!m_configured)
        wl_display_dispatch(m_display);
}

void WaylandWindow::unmap()
{
    wl_surface_attach(m_surface, nullptr, 0, 0);
    wl_surface_commit(m_surface);
    wl_display_flush(m_display);
}

void WaylandWindow::pumpEvents(std::vector<Window::Event>& eventOutput)
{
    m_event_output = &eventOutput;
    wl_display_flush(m_display);
    wl_display_dispatch_pending(m_display);
    m_event_output = nullptr;
}

MouseState WaylandWindow::get_mouse_state()
{
    return MouseState{
        .pos_x = m_mouse_x,
        .pos_y = m_mouse_y,
    };
}
} // namespace Reaper
