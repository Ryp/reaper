////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "common/ReaperRoot.h"

namespace Reaper
{
class IWindow;

class REAPER_RENDERER_API AbstractRenderer
{
public:
    virtual ~AbstractRenderer() {}

public:
    virtual void startup(IWindow* window) = 0;
    virtual void shutdown() = 0;
    virtual void render() = 0;

public:
    static AbstractRenderer* createRenderer();
};

struct VulkanBackend;

struct REAPER_RENDERER_API Renderer
{
    VulkanBackend* backend;
    IWindow*       window;
};

REAPER_RENDERER_API bool create_renderer(ReaperRoot& root);
REAPER_RENDERER_API void destroy_renderer(ReaperRoot& root);
} // namespace Reaper
