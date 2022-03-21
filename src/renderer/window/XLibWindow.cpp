////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
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
} // namespace
