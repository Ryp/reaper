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
    XCBWindow(const WindowCreationDescriptor& creationInfo);
    ~XCBWindow();

    bool renderLoop(AbstractRenderer* renderer) override;

private:
    void handleEvent(xcb_generic_event_t* event);

public:
    xcb_connection_t*   Connection;
    xcb_window_t        Handle;
    xcb_screen_t*       Screen;

private:
    xcb_intern_atom_reply_t* DeleteWindowReply;

private:
    bool ShouldResizeASAP;
    bool IsValid;
};

#endif // REAPER_XCB_WINDOW_INCLUDED

