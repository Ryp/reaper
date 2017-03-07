////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "XCBWindow.h"

#include <cstring>

#include "renderer/Renderer.h"

XCBWindow::XCBWindow(const WindowCreationDescriptor& creationInfo)
:   Connection()
,   Handle()
,   DeleteWindowReply(nullptr)
,   ShouldResizeASAP(false)
,   IsValid(false)
{
    int screen_index;

    Connection = xcb_connect( nullptr, &screen_index );
    Assert(Connection != nullptr);

    const xcb_setup_t *setup = xcb_get_setup(Connection);
    xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator( setup );

    while( screen_index-- > 0 )
        xcb_screen_next(&screen_iterator);

    Screen = screen_iterator.data;
    Assert(Screen != nullptr);

    Handle = xcb_generate_id(Connection);

    uint32_t value_list[] = {
        Screen->white_pixel,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    xcb_create_window(
        Connection,
        XCB_COPY_FROM_PARENT,
        Handle,
        Screen->root,
        20,
        20,
        creationInfo.width,
        creationInfo.height,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        Screen->root_visual,
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        value_list);

    xcb_change_property(
        Connection,
        XCB_PROP_MODE_REPLACE,
        Handle,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        std::strlen(creationInfo.title),
        creationInfo.title);

    xcb_flush(Connection);
    Assert(xcb_connection_has_error(Connection) == 0);

    IsValid = true;
}

XCBWindow::~XCBWindow()
{
    xcb_destroy_window(Connection, Handle);
    xcb_disconnect(Connection);
}

void XCBWindow::handleEvent(xcb_generic_event_t* event)
{
    switch (event->response_type & 0x7f)
    {
        case XCB_CONFIGURE_NOTIFY:
            {
                // Resize
                xcb_configure_notify_event_t *configure_event = (xcb_configure_notify_event_t*)event;
                static uint16_t width = configure_event->width;
                static uint16_t height = configure_event->height;

                if (((configure_event->width > 0) && (width != configure_event->width)) ||
                        ((configure_event->height > 0) && (height != configure_event->height)))
                {
                    ShouldResizeASAP = true;
                    width = configure_event->width;
                    height = configure_event->height;
                }
                break;
            }
        case XCB_CLIENT_MESSAGE:
            // Close
            if ((*(xcb_client_message_event_t*)event).data.data32[0] == (*DeleteWindowReply).atom)
            {
                IsValid = false;
                free(DeleteWindowReply);
            }
            break;
        case XCB_KEY_PRESS:
            IsValid = false;
            break;
    }
}

bool XCBWindow::renderLoop(AbstractRenderer* renderer)
{
    Assert(xcb_connection_has_error(Connection) == 0);

    // Prepare notification for window destruction
    xcb_intern_atom_cookie_t  protocols_cookie = xcb_intern_atom(Connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t  *protocols_reply  = xcb_intern_atom_reply(Connection, protocols_cookie, 0);

    xcb_intern_atom_cookie_t  delete_cookie    = xcb_intern_atom(Connection, 0, 16, "WM_DELETE_WINDOW");
    DeleteWindowReply = xcb_intern_atom_reply(Connection, delete_cookie, 0);

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Handle, (*protocols_reply).atom, 4, 32, 1, &(*DeleteWindowReply).atom);

    free(protocols_reply);

    // Display window
    xcb_map_window(Connection, Handle);
    xcb_flush(Connection);

    while (IsValid)
    {
        xcb_generic_event_t* event = xcb_poll_for_event(Connection);

        if (event != nullptr)
        {
            handleEvent(event);
            free(event);
        }
        else
        {
            Assert(xcb_connection_has_error(Connection) == 0);
            // Draw
            if (ShouldResizeASAP)
            {
//                 if(!renderer.OnWindowSizeChanged()) {
//                     IsValid = false;
//                 }
                ShouldResizeASAP = false;
            }
            renderer->render();
        }
    }
    return true;
}

