////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
struct GlobalResources;

void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources);
} // namespace Reaper
