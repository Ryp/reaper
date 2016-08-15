////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PresentationSurface.h"

Window::Window() :
 _parameters() {
}

WindowParameters Window::GetParameters() const {
    return _parameters;
}

#if defined(VK_USE_PLATFORM_WIN32_KHR)

#define REAPER_WINDOW_INFO "Reaper"

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {
    switch( message ) {
        case WM_SIZE:
        case WM_EXITSIZEMOVE:
            PostMessage( hWnd, WM_USER + 1, wParam, lParam );
            break;
        case WM_KEYDOWN:
        case WM_CLOSE:
            PostMessage( hWnd, WM_USER + 2, wParam, lParam );
            break;
        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }
    return 0;
}

Window::~Window() {
    if( _parameters.Handle ) {
        DestroyWindow( _parameters.Handle );
    }

    if( _parameters.Instance ) {
        UnregisterClass( REAPER_WINDOW_INFO, _parameters.Instance );
    }
}

bool Window::Create( const char *title ) {
    _parameters.Instance = GetModuleHandle( nullptr );

    // Register window class
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = _parameters.Instance;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = REAPER_WINDOW_INFO;
    wcex.hIconSm = NULL;

    if( !RegisterClassEx( &wcex ) ) {
        return false;
    }

    // Create window
    _parameters.Handle = CreateWindow( REAPER_WINDOW_INFO, title, WS_OVERLAPPEDWINDOW, 20, 20, 500, 500, nullptr, nullptr, _parameters.Instance, nullptr );
    if( !_parameters.Handle ) {
        return false;
    }

    return true;
}

bool Window::renderLoop(AbstractRenderer* renderer) const
{
    // Display window
    ShowWindow( _parameters.Handle, SW_SHOWNORMAL );
    UpdateWindow( _parameters.Handle );

    // Main message loop
    MSG message;
    bool loop = true;
    bool resize = false;

    while( loop ) {
        if( PeekMessage( &message, NULL, 0, 0, PM_REMOVE ) ) {
            // Process events
            switch( message.message ) {
                // Resize
                case WM_USER + 1:
                    resize = true;
                    break;
                    // Close
                case WM_USER + 2:
                    loop = false;
                    break;
            }
            TranslateMessage( &message );
            DispatchMessage( &message );
        } else {
            // Draw
            if( resize ) {
                resize = false;
//                 if( !renderer.OnWindowSizeChanged() ) {
//                     loop = false;
//                 }
            }
            renderer->render();
        }
    }

    return true;
}

#elif defined(VK_USE_PLATFORM_XCB_KHR)

#include <cstring>

Window::~Window()
{
    xcb_destroy_window(_parameters.Connection, _parameters.Handle);
    xcb_disconnect(_parameters.Connection);
}

bool Window::Create(const char *title)
{
    int screen_index;

    _parameters.Connection = xcb_connect( nullptr, &screen_index );
    Assert(_parameters.Connection != nullptr);

    const xcb_setup_t *setup = xcb_get_setup(_parameters.Connection);
    xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator( setup );

    while( screen_index-- > 0 )
        xcb_screen_next(&screen_iterator);

    xcb_screen_t *screen = screen_iterator.data;

    _parameters.Handle = xcb_generate_id(_parameters.Connection);

    uint32_t value_list[] = {
        screen->white_pixel,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    xcb_create_window(
        _parameters.Connection,
        XCB_COPY_FROM_PARENT,
                    _parameters.Handle,
        screen->root,
        20,
        20,
        300,
        300,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        value_list );

    xcb_change_property(
        _parameters.Connection,
        XCB_PROP_MODE_REPLACE,
                      _parameters.Handle,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        std::strlen(title),
        title);

    xcb_flush(_parameters.Connection);
    Assert(xcb_connection_has_error(_parameters.Connection) == 0);

    return true;
}

bool Window::renderLoop(AbstractRenderer* renderer) const
{
    Assert(xcb_connection_has_error(_parameters.Connection) == 0);
    // Prepare notification for window destruction
    xcb_intern_atom_cookie_t  protocols_cookie = xcb_intern_atom( _parameters.Connection, 1, 12, "WM_PROTOCOLS" );
    xcb_intern_atom_reply_t  *protocols_reply  = xcb_intern_atom_reply( _parameters.Connection, protocols_cookie, 0 );
    xcb_intern_atom_cookie_t  delete_cookie    = xcb_intern_atom( _parameters.Connection, 0, 16, "WM_DELETE_WINDOW" );
    xcb_intern_atom_reply_t  *delete_reply     = xcb_intern_atom_reply( _parameters.Connection, delete_cookie, 0 );
    xcb_change_property( _parameters.Connection, XCB_PROP_MODE_REPLACE, _parameters.Handle, (*protocols_reply).atom, 4, 32, 1, &(*delete_reply).atom );
    free( protocols_reply );

    // Display window
    xcb_map_window( _parameters.Connection, _parameters.Handle );
    xcb_flush( _parameters.Connection );

    // Main message loop
    xcb_generic_event_t *event;
    bool loop = true;
    bool resize = false;

    while( loop ) {
        event = xcb_poll_for_event( _parameters.Connection );

        if( event ) {
            // Process events
            switch (event->response_type & 0x7f) {
                // Resize
                case XCB_CONFIGURE_NOTIFY: {
                    xcb_configure_notify_event_t *configure_event = (xcb_configure_notify_event_t*)event;
                    static uint16_t width = configure_event->width;
                    static uint16_t height = configure_event->height;

                    if( ((configure_event->width > 0) && (width != configure_event->width)) ||
                        ((configure_event->height > 0) && (height != configure_event->height)) ) {
                        resize = true;
                    width = configure_event->width;
                    height = configure_event->height;
                        }
                }
                break;
                // Close
                case XCB_CLIENT_MESSAGE:
                    if( (*(xcb_client_message_event_t*)event).data.data32[0] == (*delete_reply).atom ) {
                        loop = false;
                        free( delete_reply );
                    }
                    break;
                case XCB_KEY_PRESS:
                    loop = false;
                    break;
            }
            free( event );
        } else {
            // Draw
            if( resize ) {
                resize = false;
//                 if( !renderer.OnWindowSizeChanged() ) {
//                     loop = false;
//                 }
            }
            renderer->render();
        }
    }

    return true;
}

#elif defined(VK_USE_PLATFORM_XLIB_KHR)

Window::~Window() {
    XDestroyWindow( _parameters.DisplayPtr, _parameters.Handle );
    XCloseDisplay( _parameters.DisplayPtr );
}

bool Window::Create( const char *title ) {
    _parameters.DisplayPtr = XOpenDisplay( nullptr );
    if( !_parameters.DisplayPtr ) {
        return false;
    }

    int default_screen = DefaultScreen( _parameters.DisplayPtr );

    _parameters.Handle = XCreateSimpleWindow(
        _parameters.DisplayPtr,
        DefaultRootWindow( _parameters.DisplayPtr ),
                                            20,
                                            20,
                                            500,
                                            500,
                                            1,
                                            BlackPixel( _parameters.DisplayPtr, default_screen ),
                                            WhitePixel( _parameters.DisplayPtr, default_screen ) );

    XSetStandardProperties( _parameters.DisplayPtr, _parameters.Handle, title, title, None, nullptr, 0, nullptr );
    XSelectInput( _parameters.DisplayPtr, _parameters.Handle, ExposureMask | KeyPressMask | StructureNotifyMask );

    return true;
}

bool Window::renderLoop(AbstractRenderer* renderer) const
{
    // Prepare notification for window destruction
    Atom delete_window_atom;
    delete_window_atom = XInternAtom( _parameters.DisplayPtr, "WM_DELETE_WINDOW", false );
    XSetWMProtocols( _parameters.DisplayPtr, _parameters.Handle, &delete_window_atom, 1);

    // Display window
    XClearWindow( _parameters.DisplayPtr, _parameters.Handle );
    XMapWindow( _parameters.DisplayPtr, _parameters.Handle );

    // Main message loop
    XEvent event;
    bool loop = true;
    bool resize = false;

    while( loop ) {
        if( XPending( _parameters.DisplayPtr ) ) {
            XNextEvent( _parameters.DisplayPtr, &event );
            switch( event.type ) {
                //Process events
                case ConfigureNotify: {
                    static int width = event.xconfigure.width;
                    static int height = event.xconfigure.height;

                    if( ((event.xconfigure.width > 0) && (event.xconfigure.width != width)) ||
                        ((event.xconfigure.height > 0) && (event.xconfigure.width != height)) ) {
                        width = event.xconfigure.width;
                    height = event.xconfigure.height;
                    resize = true;
                        }
                }
                break;
                case KeyPress:
                    loop = false;
                    break;
                case DestroyNotify:
                    loop = false;
                    break;
                case ClientMessage:
                    if( static_cast<unsigned int>(event.xclient.data.l[0]) == delete_window_atom ) {
                        loop = false;
                    }
                    break;
            }
        } else {
            // Draw
            if( resize ) {
                resize = false;
//                 if( !tutorial.OnWindowSizeChanged() ) {
//                     loop = false;
//                 }
            }
            renderer->render();
        }
    }

    return true;
}

#endif

using namespace vk;

void createPresentationSurface(VkInstance vkInstance, const WindowParameters& params, VkSurfaceKHR& vkPresentationSurface)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // VkStructureType                  sType
        nullptr,                                          // const void                      *pNext
        0,                                                // VkWin32SurfaceCreateFlagsKHR     flags
        params.Instance,                                  // HINSTANCE                        hinstance
        params.Handle                                     // HWND                             hwnd
    };

    Assert(vkCreateWin32SurfaceKHR(vkInstance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    VkXcbSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,    // VkStructureType                  sType
        nullptr,                                          // const void                      *pNext
        0,                                                // VkXcbSurfaceCreateFlagsKHR       flags
        params.Connection,                                // xcb_connection_t*                connection
        params.Handle                                     // xcb_window_t                     window
    };

    Assert(vkCreateXcbSurfaceKHR(vkInstance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    VkXlibSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,   // VkStructureType                sType
        nullptr,                                          // const void                    *pNext
        0,                                                // VkXlibSurfaceCreateFlagsKHR    flags
        params.DisplayPtr,                                // Display                       *dpy
        params.Handle                                     // Window                         window
    };

    Assert(vkCreateXlibSurfaceKHR(vkInstance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#endif
    Assert(vkPresentationSurface != VK_NULL_HANDLE);
}
