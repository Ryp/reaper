////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/PrepareBuckets.h"

#include <span>

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;
struct ReaperGeometry;

void allocate_scene_resources(ReaperRoot& root, VulkanBackend& backend, std::span<ReaperGeometry> geometries);

SceneGraph create_static_test_scene(ReaperRoot& root, VulkanBackend& backend);
SceneGraph create_test_scene_tiled_lighting(ReaperRoot& root, VulkanBackend& backend);
} // namespace Reaper
