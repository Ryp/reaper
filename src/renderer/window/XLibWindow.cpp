////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "XLibWindow.h"

#include <X11/Xutil.h>

#include "renderer/Renderer.h"

namespace
{
XLibWindow::XLibWindow(const WindowCreationDescriptor& creationInfo)
    : DisplayPtr()
    , Handle()
{
    DisplayPtr = XOpenDisplay(nullptr);

    Assert(DisplayPtr != nullptr);

    int default_screen = DefaultScreen(DisplayPtr);

    Handle = XCreateSimpleWindow(DisplayPtr,
                                 DefaultRootWindow(DisplayPtr),
                                 20,
                                 20,
                                 creationInfo.width,
                                 creationInfo.height,
                                 1,
                                 BlackPixel(DisplayPtr, default_screen),
                                 WhitePixel(DisplayPtr, default_screen));

    XSetStandardProperties(DisplayPtr, Handle, creationInfo.title, creationInfo.title, None, nullptr, 0, nullptr);
    XSelectInput(DisplayPtr, Handle, ExposureMask | KeyPressMask | StructureNotifyMask);
}

XLibWindow::~XLibWindow()
{
    XDestroyWindow(DisplayPtr, Handle);
    XCloseDisplay(DisplayPtr);
}

bool XLibWindow::renderLoop(AbstractRenderer* renderer)
{
    // Prepare notification for window destruction
    Atom delete_window_atom;
    delete_window_atom = XInternAtom(DisplayPtr, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(DisplayPtr, Handle, &delete_window_atom, 1);

    // Display window
    XClearWindow(DisplayPtr, Handle);
    XMapWindow(DisplayPtr, Handle);

    // Main message loop
    XEvent event;
    bool   loop = true;
    bool   resize = false;

    while (loop)
    {
        if (XPending(DisplayPtr))
        {
            XNextEvent(DisplayPtr, &event);
            switch (event.type)
            {
            // Process events
            case ConfigureNotify: {
                static int width = event.xconfigure.width;
                static int height = event.xconfigure.height;

                if (((event.xconfigure.width > 0) && (event.xconfigure.width != width))
                    || ((event.xconfigure.height > 0) && (event.xconfigure.width != height)))
                {
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
                if (static_cast<unsigned int>(event.xclient.data.l[0]) == delete_window_atom)
                {
                    loop = false;
                }
                break;
            }
        }
        else
        {
            if (resize)
            {
                // TODO Resize stuff
                resize = false;
            }

            // Draw
            renderer->render();
        }
    }
    return true;
}
} // namespace
