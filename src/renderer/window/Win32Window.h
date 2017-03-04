////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_WIN32_WINDOW_INCLUDED
#define REAPER_WIN32_WINDOW_INCLUDED

#include <Windows.h>

#include "Window.h"

class REAPER_RENDERER_API Win32Window : public IWindow
{
public:
    Win32Window();
    ~Win32Window();

    bool create(const char* title) override;
    bool renderLoop(AbstractRenderer* renderer) const override;

public:
    HINSTANCE   Instance;
    HWND        Handle;
};

#endif // REAPER_WIN32_WINDOW_INCLUDED


