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

REAPER_RENDERER_API
void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend);
} // namespace Reaper
