////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/RendererExport.h"

#include <core/Assert.h>
#include <core/Types.h>

#include <vector>

namespace Reaper
{
namespace Window
{
    struct Event;
}

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
