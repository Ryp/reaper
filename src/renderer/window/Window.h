////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

namespace Reaper
{
namespace Window
{
    struct Event;
}

class AbstractRenderer;

class REAPER_RENDERER_API IWindow
{
public:
    virtual ~IWindow() {}
    virtual void map() = 0;
    virtual void unmap() = 0;
    virtual void pumpEvents(std::vector<Window::Event>& eventOutput) = 0;
};

struct WindowCreationDescriptor
{
    const char* title;
    u32         width;
    u32         height;
    bool        fullscreen;
};

REAPER_RENDERER_API IWindow* createWindow(const WindowCreationDescriptor& creationInfo);
} // namespace Reaper
