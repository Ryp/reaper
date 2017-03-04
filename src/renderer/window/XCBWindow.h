////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_XCB_WINDOW_INCLUDED
#define REAPER_XCB_WINDOW_INCLUDED

#include <xcb/xcb.h>

#include "Window.h"

class REAPER_RENDERER_API XCBWindow : public IWindow
{
public:
    XCBWindow();
    ~XCBWindow();

    bool create(const char* title) override;
    bool renderLoop(AbstractRenderer* renderer) const override;

public:
    xcb_connection_t*   Connection;
    xcb_window_t        Handle;
};

#endif // REAPER_XCB_WINDOW_INCLUDED

