////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/mat4x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace Reaper
{
struct RendererViewport // FIXME name
{
    glm::uvec2 extent;
    float      aspect_ratio;
};

RendererViewport build_renderer_viewport(glm::uvec2 extent);

struct RendererPerspectiveProjection // FIXME name
{
    glm::mat4 vs_to_cs_matrix; // Commonly projection matrix
    glm::mat4 cs_to_vs_matrix;

    float near_plane_distance;
    float far_plane_distance;
    float half_fov_horizontal_radian;
    float half_fov_vertical_radian;
    bool  use_reverse_z;
};

RendererPerspectiveProjection build_renderer_perspective_projection(float aspect_ratio,
                                                                    float near_plane_distance,
                                                                    float far_plane_distance,
                                                                    float half_fov_horizontal_radian,
                                                                    bool  use_reverse_z);

struct RendererPerspectiveCamera // FIXME name
{
    glm::mat4x3 vs_to_ws_matrix;
    glm::mat4x3 ws_to_vs_matrix; // Commonly view matrix

    RendererViewport              viewport; // FIXME split?
    RendererPerspectiveProjection perspective_projection;

    glm::mat4 ws_to_cs_matrix; // Commonly view proj matrix
    glm::mat4 cs_to_ws_matrix;
};

RendererPerspectiveCamera build_renderer_perspective_camera(glm::mat4x3                          transform,
                                                            const RendererPerspectiveProjection& perspective_projection,
                                                            const RendererViewport&              viewport);
} // namespace Reaper
