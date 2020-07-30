////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PrepareBuckets.h"

#include <glm/gtc/matrix_transform.hpp>

#include "math/Constants.h"

namespace Reaper
{
constexpr bool UseReverseZ = true;
constexpr u32  MeshInstanceCount = 6;

void build_scene_graph(SceneGraph& scene)
{
    {
        // Dummy node
        Node node = {};
        node.mesh = (GirugaMesh*)0xFF; // FIXME get it from the asset streamer or something

        for (u32 i = 0; i < MeshInstanceCount; i++)
            scene.nodes.push_back(node);
    }

    {
        // Add to scene
        Node light_node = {};
        scene.nodes.push_back(light_node);

        Light main_light;

        main_light.color = glm::fvec3(0.8f, 0.5f, 0.2f);
        main_light.intensity = 8.f;
        main_light.scene_node = scene.nodes.size() - 1; // FIXME

        scene.lights.push_back(main_light);
    }

    {
        // Dummy node
        Node camera_node = {};
        scene.nodes.push_back(camera_node);

        scene.camera.scene_node = scene.nodes.size() - 1; // FIXME
    }
}

namespace
{
    glm::mat4 build_perspective_matrix(float near_plane, float far_plane, float aspect_ratio, float fov_radian)
    {
        glm::mat4 projection = glm::perspective(fov_radian, aspect_ratio, near_plane, far_plane);

        // Flip viewport Y
        projection[1] = -projection[1];

        if (UseReverseZ)
        {
            // NOTE: we might want to do it by hand to limit precision loss
            glm::mat4 reverse_z_transform(1.f);
            reverse_z_transform[3][2] = 1.f;
            reverse_z_transform[2][2] = -1.f;

            projection = reverse_z_transform * projection;
        }

        return projection;
    }
} // namespace

void update_scene_graph(SceneGraph& scene, float time_ms, float aspect_ratio, const glm::mat4x3& view_matrix)
{
    // Update meshes
    for (u32 i = 0; i < MeshInstanceCount; i++)
    {
        const float     ratio = static_cast<float>(i) / static_cast<float>(MeshInstanceCount) * Math::Pi * 2.f;
        const glm::vec3 object_position_ws = glm::vec3(glm::cos(ratio + time_ms), glm::cos(ratio), glm::sin(ratio));
        // const float     uniform_scale = 0.0005f;
        const float     uniform_scale = 1.0f;
        const glm::mat4 model = glm::rotate(glm::scale(glm::translate(glm::mat4(1.0f), object_position_ws),
                                                       glm::vec3(uniform_scale, uniform_scale, uniform_scale)),
                                            time_ms + ratio, glm::vec3(0.f, 1.f, 1.f));

        scene.nodes[i].transform_matrix = glm::mat4x3(model);
    }

    // Update camera node
    {
        const float near_plane_distance = 0.1f;
        const float far_plane_distance = 100.f;
        const float fov_radian = glm::pi<float>() * 0.25f;

        scene.camera.projection_matrix =
            build_perspective_matrix(near_plane_distance, far_plane_distance, aspect_ratio, fov_radian);

        Node& camera_node = scene.nodes[scene.camera.scene_node];
        camera_node.transform_matrix = view_matrix;
    }

    for (Light& light : scene.lights)
    {
        light.projection_matrix = build_perspective_matrix(0.1f, 100.f, 1.f, glm::pi<float>() * 0.25f);

        const glm::vec3   up_ws = glm::vec3(0.f, 1.f, 0.f);
        const glm::vec3   light_position_ws = glm::vec3(-1.f, 1.f, 1.f);
        const glm::mat4x3 light_transform = glm::lookAt(light_position_ws, glm::vec3(0.f, 0.f, 0.f), up_ws);

        Node& light_node = scene.nodes[light.scene_node];
        light_node.transform_matrix = light_transform;
    }
}

void prepare_scene(SceneGraph& scene, PreparedData& prepared)
{
    const Node& camera_node = scene.nodes[scene.camera.scene_node];
    const Node& light_node = scene.nodes[scene.lights.front().scene_node];

    // Main + culling pass
    const glm::mat4 main_camera_view_proj = scene.camera.projection_matrix * glm::mat4(camera_node.transform_matrix);

    prepared.draw_pass_params.view = camera_node.transform_matrix;
    prepared.draw_pass_params.proj = scene.camera.projection_matrix;
    prepared.draw_pass_params.view_proj = main_camera_view_proj;

    {
        const glm::vec3 light_position_ws =
            glm::inverse(glm::mat4(light_node.transform_matrix)) * glm::vec4(0.f, 0.f, 0.f, 1.0f);
        const glm::vec3 light_position_vs = camera_node.transform_matrix * glm::fvec4(light_position_ws, 1.f);

        prepared.draw_pass_params.point_light.position_vs = light_position_vs;
        prepared.draw_pass_params.point_light.intensity = 8.f;
        prepared.draw_pass_params.point_light.color = glm::fvec3(0.8f, 0.5f, 0.2f);
    }

    for (const auto& node : scene.nodes)
    {
        if (node.mesh == nullptr)
            continue;

        // Assumption that our 3x3 submatrix is orthonormal (no skew/non-uniform scaling)
        // FIXME use 4x3 matrices directly
        const glm::mat4x3 modelView = glm::mat4(camera_node.transform_matrix) * glm::mat4(node.transform_matrix);

        DrawInstanceParams draw_instance;
        draw_instance.model = node.transform_matrix, draw_instance.normal_ms_to_vs_matrix = glm::mat3(modelView);

        prepared.draw_instance_params.push_back(draw_instance);

        CullInstanceParams cull_instance;
        cull_instance.ms_to_cs_matrix = main_camera_view_proj * glm::mat4(node.transform_matrix);

        prepared.cull_instance_params.push_back(cull_instance);
    }

    // Shadow pass
    prepared.shadow_pass_params.dummy = glm::mat4(1.f);

    for (const auto& node : scene.nodes)
    {
        if (node.mesh == nullptr)
            continue;

        const glm::mat4 light_view_proj_matrix =
            scene.lights.front().projection_matrix * glm::mat4(light_node.transform_matrix);

        ShadowMapInstanceParams shadow_instance;
        shadow_instance.ms_to_cs_matrix = light_view_proj_matrix * glm::mat4(node.transform_matrix);

        prepared.shadow_instance_params.push_back(shadow_instance);
    }
}
} // namespace Reaper
