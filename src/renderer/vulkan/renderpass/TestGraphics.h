////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/RendererExport.h"

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
struct IWindow;
struct BackendResources;

REAPER_RENDERER_API bool vulkan_process_window_events(ReaperRoot& root, VulkanBackend& backend, IWindow* window);
void                     resize_swapchain(ReaperRoot& root, VulkanBackend& backend, BackendResources& resources);

struct CommandBuffer;
struct PreparedData;

void backend_execute_frame(ReaperRoot& root, VulkanBackend& backend, CommandBuffer& cmdBuffer,
                           const PreparedData& prepared, BackendResources& resources);

REAPER_RENDERER_API void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend);
} // namespace Reaper
