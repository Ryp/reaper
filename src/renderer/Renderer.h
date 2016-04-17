////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_RENDERER_INCLUDED
#define REAPER_RENDERER_INCLUDED

class Window;

class AbstractRenderer
{
public:
    virtual ~AbstractRenderer() {}

public:
    virtual void startup(Window* window) = 0;
    virtual void shutdown() = 0;
    virtual void render() = 0;

public:
    static AbstractRenderer* createRenderer();
};

#endif // REAPER_RENDERER_INCLUDED
