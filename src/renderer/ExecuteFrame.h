////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RendererExport.h"

#include <core/Types.h>

#include <span>
#include <vector>

struct DebugGeometryUserCommand;

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
class IWindow;

REAPER_RENDERER_API void renderer_start(ReaperRoot& root, VulkanBackend& backend, IWindow* window);
REAPER_RENDERER_API void renderer_stop(ReaperRoot& root, VulkanBackend& backend, IWindow* window);

struct SceneGraph;

REAPER_RENDERER_API void
renderer_execute_frame(ReaperRoot& root, const SceneGraph& scene, std::vector<u8>& audio_output,
                       std::span<DebugGeometryUserCommand> debug_draw_commands = std::span<DebugGeometryUserCommand>());
} // namespace Reaper
