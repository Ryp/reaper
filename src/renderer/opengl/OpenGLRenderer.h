////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_OPENGLRENDERER_INCLUDED
#define REAPER_OPENGLRENDERER_INCLUDED

#include "renderer/Renderer.h"

class OpenGLRenderer : public AbstractRenderer
{
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() = default;

public:
    void startup(Window* window) override;
    void shutdown() override;
    void render() override;
};

#endif // REAPER_OPENGLRENDERER_INCLUDED
