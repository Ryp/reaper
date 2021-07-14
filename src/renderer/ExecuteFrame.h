////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
struct ReaperRoot;
struct SceneGraph;
struct CameraState;

REAPER_RENDERER_API void renderer_execute_frame(ReaperRoot& root, SceneGraph& scene, const CameraState& camera_state,
                                                u32 frameIndex, float timeMs);
} // namespace Reaper
