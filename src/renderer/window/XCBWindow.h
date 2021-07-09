////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <xcb/xcb.h>

#include "Window.h"

namespace Reaper
{
class REAPER_RENDERER_API XCBWindow : public IWindow
{
public:
    XCBWindow(const WindowCreationDescriptor& creationInfo);
    ~XCBWindow();

    void map() override final;
    void unmap() override final;
    void pumpEvents(std::vector<Window::Event>& eventOutput) override final;

public:
    xcb_connection_t* m_connection;
    xcb_window_t      m_handle;
    xcb_screen_t*     m_screen;

private:
    xcb_intern_atom_reply_t* m_deleteWindowReply;
};
} // namespace Reaper
