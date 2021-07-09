////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "XCBWindow.h"

#include "Event.h"

#include <cstring>

#include "renderer/Renderer.h"

namespace Reaper
{
XCBWindow::XCBWindow(const WindowCreationDescriptor& creationInfo)
    : m_connection()
    , m_handle()
    , m_deleteWindowReply(nullptr)
{
    int screen_index;

    m_connection = xcb_connect(nullptr, &screen_index);
    Assert(m_connection != nullptr);

    const xcb_setup_t*    setup = xcb_get_setup(m_connection);
    xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(setup);

    while (screen_index-- > 0)
        xcb_screen_next(&screen_iterator);

    m_screen = screen_iterator.data;
    Assert(m_screen != nullptr);

    m_handle = xcb_generate_id(m_connection);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t value_list[] = {m_screen->white_pixel,
                             XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY};

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

    xcb_change_property(
        m_connection, XCB_PROP_MODE_REPLACE, m_handle, (*protocols_reply).atom, 4, 32, 1, &(*m_deleteWindowReply).atom);

    xcb_void_cookie_t    cookie = xcb_map_window_checked(m_connection, m_handle);
    xcb_generic_error_t* error = nullptr;

    if ((error = xcb_request_check(m_connection, cookie)))
    {
        fprintf(stderr, "Could not reparent the window\n");
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
        fprintf(stderr, "Could not reparent the window\n");
        free(error);
    }
    Assert(xcb_connection_has_error(m_connection) == 0);
}

namespace
{
    Window::Event convertXcbEvent(xcb_generic_event_t* event)
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
        case XCB_KEY_PRESS:
            return Window::createKeyPressEvent(42);
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
            eventOutput.emplace_back(convertXcbEvent(event));
            free(event);
        }
    } while (event);
}
} // namespace Reaper
