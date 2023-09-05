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
struct VulkanBackend;

SceneGraph create_static_test_scene(VulkanBackend& backend);
SceneGraph create_test_scene_tiled_lighting(VulkanBackend& backend);
} // namespace Reaper
