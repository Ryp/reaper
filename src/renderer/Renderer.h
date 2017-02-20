////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_RENDERER_INCLUDED
#define REAPER_RENDERER_INCLUDED

class Window;

class REAPER_RENDERER_API AbstractRenderer
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

#include "common/ReaperRoot.h"

REAPER_RENDERER_API bool create_renderer(ReaperRoot* root);
REAPER_RENDERER_API void destroy_renderer(ReaperRoot* root);

#endif // REAPER_RENDERER_INCLUDED

