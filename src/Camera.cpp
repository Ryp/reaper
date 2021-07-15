////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Reaper
{
glm::mat4 compute_camera_view_matrix(const CameraState& state)
{
    const float cos_yaw = glm::cos(state.yaw);
    const float sin_yaw = glm::sin(state.yaw);
    const float cos_pitch = glm::cos(state.pitch);
    const float sin_pitch = glm::sin(state.pitch);

    const glm::vec3 direction = glm::vec3(cos_yaw * cos_pitch, sin_pitch, sin_yaw * cos_pitch);
    return glm::lookAt(state.position, state.position + direction * 10.f, glm::vec3(0, 1, 0));
}

void update_camera_state(CameraState& state, glm::vec2 yaw_pitch_delta, glm::vec2 forward_side_delta)
{
    // FIXME cache this info
    const glm::mat4 previous_view_matrix = compute_camera_view_matrix(state);

    constexpr float yaw_sensitivity = 2.6f;
    constexpr float pitch_sensitivity = 2.5f; // radian per sec
    constexpr float translation_speed = 8.0f; // game units per sec

    const glm::mat4 previous_view_matrix_inv = glm::inverse(previous_view_matrix);
    const glm::vec3 forward = previous_view_matrix_inv * glm::vec4(0.f, 0.f, 1.f, 0.f);
    const glm::vec3 side = previous_view_matrix_inv * glm::vec4(1.f, 0.f, 0.f, 0.f);

    glm::vec3   translation = forward * forward_side_delta.x + side * forward_side_delta.y;
    const float magnitudeSq = glm::dot(translation, translation);

    if (magnitudeSq > 1.f)
        translation *= 1.f / glm::sqrt(magnitudeSq);

    const glm::vec3 camera_offset = translation * translation_speed;

    state.position += camera_offset;
    state.yaw += yaw_pitch_delta.x * yaw_sensitivity;
    state.pitch += yaw_pitch_delta.y * pitch_sensitivity;
}
} // namespace Reaper
