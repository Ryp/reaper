////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Reaper
{
namespace
{
    glm::mat4 apply_reverse_z_fixup(glm::mat4 projection_matrix, bool reverse_z)
    {
        if (reverse_z)
        {
            // NOTE: we might want to do it by hand to limit precision loss
            glm::mat4 reverse_z_transform(1.f);
            reverse_z_transform[3][2] = 1.f;
            reverse_z_transform[2][2] = -1.f;

            return reverse_z_transform * projection_matrix;
        }
        else
        {
            return projection_matrix;
        }
    }

    glm::mat4 build_perspective_matrix(float near_plane, float far_plane, float aspect_ratio, float fov_radian)
    {
        glm::mat4 projection = glm::perspective(fov_radian, aspect_ratio, near_plane, far_plane);

        // Flip viewport Y
        projection[1] = -projection[1];

        return projection;
    }
} // namespace

RendererViewport build_renderer_viewport(glm::uvec2 extent)
{
    RendererViewport viewport;

    viewport.extent = extent;
    viewport.aspect_ratio = static_cast<float>(extent.x) / static_cast<float>(extent.y);

    return viewport;
}

RendererPerspectiveProjection build_renderer_perspective_projection(float aspect_ratio,
                                                                    float near_plane_distance,
                                                                    float far_plane_distance,
                                                                    float half_fov_horizontal_radian,
                                                                    bool  use_reverse_z)
{
    RendererPerspectiveProjection projection;

    const glm::mat4 projection_matrix = apply_reverse_z_fixup(
        build_perspective_matrix(near_plane_distance, far_plane_distance, aspect_ratio, half_fov_horizontal_radian),
        use_reverse_z);

    projection.vs_to_cs_matrix = projection_matrix;
    projection.cs_to_vs_matrix = glm::inverse(projection_matrix);

    projection.near_plane_distance = near_plane_distance;
    projection.far_plane_distance = far_plane_distance;
    projection.half_fov_horizontal_radian = half_fov_horizontal_radian;
    projection.half_fov_vertical_radian = std::atan(std::tan(half_fov_horizontal_radian) * aspect_ratio);
    projection.use_reverse_z = use_reverse_z;

    return projection;
}

RendererPerspectiveCamera build_renderer_perspective_camera(glm::mat4x3                          transform,
                                                            const RendererPerspectiveProjection& perspective_projection,
                                                            const RendererViewport&              viewport)
{
    RendererPerspectiveCamera camera;

    camera.vs_to_ws_matrix = transform;
    camera.ws_to_vs_matrix = glm::inverse(glm::mat4(transform));

    camera.viewport = viewport;
    camera.perspective_projection = perspective_projection;

    camera.ws_to_cs_matrix = perspective_projection.vs_to_cs_matrix * glm::mat4(camera.ws_to_vs_matrix);
    camera.cs_to_ws_matrix = glm::mat4(camera.vs_to_ws_matrix) * perspective_projection.cs_to_vs_matrix;

    return camera;
}
} // namespace Reaper
