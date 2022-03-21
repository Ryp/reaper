////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Window.h"

#include <X11/Xlib.h>

namespace
{
class REAPER_RENDERER_API XLibWindow : public IWindow
{
public:
    XLibWindow(const WindowCreationDescriptor& creationInfo);
    ~XLibWindow();

public:
    Display* DisplayPtr;
    Window   Handle;
};
} // namespace
