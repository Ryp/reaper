////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "glm/mat4x4.hpp"
#include "glm/vec3.hpp"

namespace Reaper
{
struct CameraState
{
    glm::vec3 position;
    float     yaw;
    float     pitch;
};

glm::mat4                compute_camera_view_matrix(const CameraState& state);
REAPER_RENDERER_API void update_camera_state(CameraState& state, glm::vec2 yaw_pitch_delta,
                                             glm::vec2 forward_side_delta);
} // namespace Reaper
