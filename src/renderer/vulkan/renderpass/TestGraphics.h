////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/RendererExport.h"

struct ImDrawData;

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
struct BackendResources;
class IWindow;

void resize_swapchain(ReaperRoot& root, VulkanBackend& backend);

struct CommandBuffer;
struct PreparedData;

REAPER_RENDERER_API void backend_debug_ui(VulkanBackend& backend);

struct TiledLightingFrame;

void backend_execute_frame(ReaperRoot& root, VulkanBackend& backend, CommandBuffer& cmdBuffer,
                           const PreparedData& prepared, const TiledLightingFrame& tiled_lighting_frame,
                           BackendResources& resources, ImDrawData* imgui_draw_data);
} // namespace Reaper
