////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_WINDOW_INCLUDED
#define REAPER_WINDOW_INCLUDED

class AbstractRenderer;

class REAPER_RENDERER_API IWindow
{
public:
    virtual ~IWindow() {}
    virtual bool renderLoop(AbstractRenderer* renderer) = 0;
};

struct WindowCreationDescriptor
{
    const char* title;
    u32         width;
    u32         height;
    bool        fullscreen;
};

REAPER_RENDERER_API IWindow* createWindow(const WindowCreationDescriptor& creationInfo);

#endif // REAPER_WINDOW_INCLUDED

