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
struct VulkanBackend;
class IWindow;

struct REAPER_RENDERER_API Renderer
{
    VulkanBackend* backend;
    IWindow*       window;
};

REAPER_RENDERER_API void create_renderer(ReaperRoot& root);
REAPER_RENDERER_API void destroy_renderer(ReaperRoot& root);
} // namespace Reaper
