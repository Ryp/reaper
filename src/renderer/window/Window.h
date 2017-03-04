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
    virtual bool create(const char *title) = 0;
    virtual bool renderLoop(AbstractRenderer* renderer) const = 0;
};

REAPER_RENDERER_API IWindow* createWindow();

#endif // REAPER_WINDOW_INCLUDED

