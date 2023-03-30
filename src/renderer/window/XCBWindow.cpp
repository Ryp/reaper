////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// See also:
// https://www.x.org/releases/X11R7.5/doc/libxcb/tutorial/#xevents
#include "XCBWindow.h"

#include "Event.h"

// XCB has a C interface but uses the C++-reserved explicit keyword.
// This workaround lets us compile without having to wait on the project to address the issue.
#define XCB_UGLY_DEFINE_HACK 1

#if XCB_UGLY_DEFINE_HACK
#    ifdef __clang__
#        pragma clang diagnostic push
#        pragma clang diagnostic ignored "-Wkeyword-macro"
#    endif
#    define explicit explicit_
#    ifdef __clang__
#        pragma clang diagnostic pop
#    endif

#    include <xcb/xkb.h>

#    undef explicit
#endif // XCB_UGLY_DEFINE_HACK

#include <cstring>

#include "renderer/Renderer.h"

namespace Reaper
{
namespace
{
    constexpr bool InitializeXKBAutoRepeatSettings = true;
}

namespace Window
{
    namespace
    {
        MouseButton::type convert_xcb_mouse_button(xcb_button_t button)
        {
            switch (button)
            {
            case XCB_BUTTON_INDEX_1:
                return MouseButton::Left;
            case XCB_BUTTON_INDEX_2:
                return MouseButton::Middle;
            case XCB_BUTTON_INDEX_3:
                return MouseButton::Right;
            case XCB_BUTTON_INDEX_4:
                return MouseButton::WheelUp;
            case XCB_BUTTON_INDEX_5:
                return MouseButton::WheelDown;
            case 6:
                return MouseButton::WheelLeft; // NOTE: this is not documented
            case 7:
                return MouseButton::WheelRight; // NOTE: this is not documented
            default:
                AssertUnreachable();
                return MouseButton::Invalid;
            }
        }

        KeyCode::type convert_xcb_keycode(xcb_keycode_t key_code)
        {
            // FIXME this is ignoring a lot of layers from the Xlib code
            constexpr u32 XCB_KEY_CODE_ESCAPE = 9; // XK_Escape
            constexpr u32 XCB_KEY_CODE_RETURN = 36;
            constexpr u32 XCB_KEY_CODE_SPACE = 65;
            constexpr u32 XCB_KEY_CODE_ARROW_RIGHT = 114;
            constexpr u32 XCB_KEY_CODE_ARROW_LEFT = 113;
            constexpr u32 XCB_KEY_CODE_ARROW_DOWN = 116;
            constexpr u32 XCB_KEY_CODE_ARROW_UP = 111;
            constexpr u32 XCB_KEY_CODE_W = 25;
            constexpr u32 XCB_KEY_CODE_A = 38;
            constexpr u32 XCB_KEY_CODE_S = 39;
            constexpr u32 XCB_KEY_CODE_D = 40;
            constexpr u32 XCB_KEY_CODE_1 = 10;
            constexpr u32 XCB_KEY_CODE_2 = 11;
            constexpr u32 XCB_KEY_CODE_3 = 12;
            constexpr u32 XCB_KEY_CODE_4 = 13;
            constexpr u32 XCB_KEY_CODE_5 = 14;
            constexpr u32 XCB_KEY_CODE_6 = 15;
            constexpr u32 XCB_KEY_CODE_7 = 16;
            constexpr u32 XCB_KEY_CODE_8 = 17;
            constexpr u32 XCB_KEY_CODE_9 = 18;
            constexpr u32 XCB_KEY_CODE_0 = 19;

            switch (key_code)
            {
            case XCB_KEY_CODE_ESCAPE:
                return KeyCode::ESCAPE;
            case XCB_KEY_CODE_RETURN:
                return KeyCode::ENTER;
            case XCB_KEY_CODE_SPACE:
                return KeyCode::SPACE;
            case XCB_KEY_CODE_ARROW_RIGHT:
                return KeyCode::ARROW_RIGHT;
            case XCB_KEY_CODE_ARROW_LEFT:
                return KeyCode::ARROW_LEFT;
            case XCB_KEY_CODE_ARROW_DOWN:
                return KeyCode::ARROW_DOWN;
            case XCB_KEY_CODE_ARROW_UP:
                return KeyCode::ARROW_UP;
            case XCB_KEY_CODE_W:
                return KeyCode::W;
            case XCB_KEY_CODE_A:
                return KeyCode::A;
            case XCB_KEY_CODE_S:
                return KeyCode::S;
            case XCB_KEY_CODE_D:
                return KeyCode::D;
            case XCB_KEY_CODE_1:
                return KeyCode::NUM_1;
            case XCB_KEY_CODE_2:
                return KeyCode::NUM_2;
            case XCB_KEY_CODE_3:
                return KeyCode::NUM_3;
            case XCB_KEY_CODE_4:
                return KeyCode::NUM_4;
            case XCB_KEY_CODE_5:
                return KeyCode::NUM_5;
            case XCB_KEY_CODE_6:
                return KeyCode::NUM_6;
            case XCB_KEY_CODE_7:
                return KeyCode::NUM_7;
            case XCB_KEY_CODE_8:
                return KeyCode::NUM_8;
            case XCB_KEY_CODE_9:
                return KeyCode::NUM_9;
            case XCB_KEY_CODE_0:
                return KeyCode::NUM_0;
            default:
                return KeyCode::Invalid;
            }
        }
    } // namespace
} // namespace Window

XCBWindow::XCBWindow(const WindowCreationDescriptor& creationInfo)
    : m_connection()
    , m_handle()
    , m_deleteWindowReply(nullptr)
{
    int screen_index;

    m_connection = xcb_connect(nullptr, &screen_index);
    Assert(m_connection != nullptr);

    if (InitializeXKBAutoRepeatSettings)
    {
        xcb_xkb_use_extension_cookie_t use_extension_cookie =
            xcb_xkb_use_extension(m_connection, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);

        {
            xcb_generic_error_t*           error = nullptr;
            xcb_xkb_use_extension_reply_t* use_extension_reply =
                xcb_xkb_use_extension_reply(m_connection, use_extension_cookie, &error);

            if (error)
            {
                Assert(false, "Could not enable XKB extension");
                free(error);
            }

            Assert(use_extension_reply->supported);
        }

        {
            u32 flags = XCB_XKB_PER_CLIENT_FLAG_GRABS_USE_XKB_STATE | XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT;

            // Set the per client detectable auto repeat flag
            xcb_xkb_per_client_flags_cookie_t set_flags_cookie =
                xcb_xkb_per_client_flags(m_connection, XCB_XKB_ID_USE_CORE_KBD, flags, 1, 0, 0, 0);

            xcb_generic_error_t*              error = nullptr;
            xcb_xkb_per_client_flags_reply_t* set_flags_reply =
                xcb_xkb_per_client_flags_reply(m_connection, set_flags_cookie, &error);

            if (error)
            {
                Assert(false, "Could not set XKB flags");
                free(error);
            }

            Assert((set_flags_reply->value & XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT) != 0);
        }
    }

    const xcb_setup_t*    setup = xcb_get_setup(m_connection);
    xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(setup);

    while (screen_index-- > 0)
        xcb_screen_next(&screen_iterator);

    m_screen = screen_iterator.data;
    Assert(m_screen != nullptr);

    m_handle = xcb_generate_id(m_connection);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t value_list[] = {m_screen->black_pixel, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS
                                                        | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_KEY_PRESS
                                                        | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_STRUCTURE_NOTIFY};

    xcb_create_window(m_connection,
                      XCB_COPY_FROM_PARENT,
                      m_handle,
                      m_screen->root,
                      0,
                      0,
                      creationInfo.width,
                      creationInfo.height,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      m_screen->root_visual,
                      mask,
                      value_list);

    xcb_change_property(m_connection,
                        XCB_PROP_MODE_REPLACE,
                        m_handle,
                        XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING,
                        8,
                        std::strlen(creationInfo.title),
                        creationInfo.title);

    xcb_flush(m_connection);
    Assert(xcb_connection_has_error(m_connection) == 0);
}

XCBWindow::~XCBWindow()
{
    xcb_destroy_window(m_connection, m_handle);
    xcb_disconnect(m_connection);
}

void XCBWindow::map()
{
    // Prepare notification for window destruction
    xcb_intern_atom_cookie_t protocols_cookie = xcb_intern_atom(m_connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* protocols_reply = xcb_intern_atom_reply(m_connection, protocols_cookie, 0);

    xcb_intern_atom_cookie_t delete_cookie = xcb_intern_atom(m_connection, 0, 16, "WM_DELETE_WINDOW");
    m_deleteWindowReply = xcb_intern_atom_reply(m_connection, delete_cookie, 0);

    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_handle, (*protocols_reply).atom, 4, 32, 1,
                        &(*m_deleteWindowReply).atom);

    xcb_void_cookie_t    cookie = xcb_map_window_checked(m_connection, m_handle);
    xcb_generic_error_t* error = nullptr;

    if ((error = xcb_request_check(m_connection, cookie)))
    {
        Assert(false, "Could not map the window");
        free(error);
    }

    Assert(xcb_connection_has_error(m_connection) == 0);
    xcb_flush(m_connection);
}

void XCBWindow::unmap()
{
    xcb_void_cookie_t    cookie = xcb_unmap_window_checked(m_connection, m_handle);
    xcb_generic_error_t* error = nullptr;

    if ((error = xcb_request_check(m_connection, cookie)))
    {
        Assert(false, "Could not unmap the window");
        free(error);
    }

    Assert(xcb_connection_has_error(m_connection) == 0);
}

namespace
{
    Window::Event convert_xcb_event(xcb_generic_event_t* event)
    {
        constexpr u32 xcbMagicMask = 0x7f;
        switch (event->response_type & xcbMagicMask)
        {
        case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t* configure_event = (xcb_configure_notify_event_t*)event;
            // Extent can be zero!
            // example: running the current window in fullscreen mode while
            //          trying to open a new one under i3
            // Assert(configure_event->width > 0);
            // Assert(configure_event->height > 0);

            return Window::createResizeEvent(configure_event->width, configure_event->height);
        }
        case XCB_CLIENT_MESSAGE: {
            // Close
            /*
            if ((*(xcb_client_message_event_t*)event).data.data32[0] == (*m_deleteWindowReply).atom)
            {
                // CLOSE
                free(m_deleteWindowReply);
            }
            */
            break;
        }
        case XCB_REPARENT_NOTIFY: {
            // xcb_reparent_notify_event_t* reparent_event = (xcb_reparent_notify_event_t*)event;
            // AssertUnreachable();
            break;
        }
        case XCB_BUTTON_PRESS: {
            xcb_button_press_event_t* button_event = (xcb_button_press_event_t*)event;
            return Window::createButtonEvent(Window::convert_xcb_mouse_button(button_event->detail), true);
        }
        case XCB_BUTTON_RELEASE: {
            const xcb_button_release_event_t* button_event = (xcb_button_release_event_t*)event;
            return Window::createButtonEvent(Window::convert_xcb_mouse_button(button_event->detail), false);
        }
        case XCB_KEY_PRESS: {
            xcb_key_press_event_t* key_event = (xcb_key_press_event_t*)event;
            xcb_keycode_t          key_code = key_event->detail;
            return Window::createKeyEvent(Window::convert_xcb_keycode(key_code), true, key_code);
        }
        case XCB_KEY_RELEASE: {
            xcb_key_release_event_t* key_event = (xcb_key_release_event_t*)event;
            xcb_keycode_t            key_code = key_event->detail;
            return Window::createKeyEvent(Window::convert_xcb_keycode(key_code), false, key_code);
        }
        default:
            // AssertUnreachable();
            break;
        }
        // AssertUnreachable();
        Window::Event invalidEvent = {};
        return invalidEvent;
    }
} // namespace

void XCBWindow::pumpEvents(std::vector<Window::Event>& eventOutput)
{
    xcb_generic_event_t* event = nullptr;

    do
    {
        Assert(xcb_connection_has_error(m_connection) == 0);

        event = xcb_poll_for_event(m_connection);

        if (event != nullptr)
        {
            eventOutput.emplace_back(convert_xcb_event(event));
            free(event);
        }
    } while (event);
}

MouseState XCBWindow::get_mouse_state()
{
    MouseState mouse_state;

    xcb_query_pointer_cookie_t c = xcb_query_pointer(m_connection, m_handle);

    xcb_generic_error_t*       error = nullptr;
    xcb_query_pointer_reply_t* t = xcb_query_pointer_reply(m_connection, c, &error);
    mouse_state.pos_x = t->win_x;
    mouse_state.pos_y = t->win_y;

    return mouse_state;
}
} // namespace Reaper
