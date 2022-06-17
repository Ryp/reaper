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

void backend_execute_frame(ReaperRoot& root, VulkanBackend& backend, CommandBuffer& cmdBuffer,
                           const PreparedData& prepared, BackendResources& resources, ImDrawData* imgui_draw_data);
} // namespace Reaper
