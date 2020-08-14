////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PrepareBuckets.h"

#include "renderer/vulkan/renderpass/CullingConstants.h"

#include "mesh/Mesh.h"

#include <glm/gtc/matrix_transform.hpp>

#include "math/Constants.h"

namespace Reaper
{
constexpr bool UseReverseZ = true;
constexpr u32  MeshInstanceCount = 6;
constexpr u32  InvalidMeshInstanceId = -1;

void build_scene_graph(SceneGraph& scene, const Mesh* mesh)
{
    for (u32 i = 0; i < MeshInstanceCount; i++)
    {
        Node& node = scene.nodes.emplace_back();
        node.instance_id = i;
        node.mesh = mesh;
    }

    {
        // Add to scene
        Node&     light_node = scene.nodes.emplace_back();
        const u32 light_node_index = scene.nodes.size() - 1;

        light_node.instance_id = InvalidMeshInstanceId;

        Light& main_light = scene.lights.emplace_back();
        main_light.color = glm::fvec3(0.8f, 0.5f, 0.2f);
        main_light.intensity = 8.f;
        main_light.scene_node = light_node_index;
    }

    {
        // Dummy node
        Node&     camera_node = scene.nodes.emplace_back();
        const u32 camera_node_index = scene.nodes.size() - 1;

        camera_node.instance_id = InvalidMeshInstanceId;

        scene.camera.scene_node = camera_node_index;
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

namespace
{
    void insert_cull_cmd(CullPassData& cull_pass, const Node& node, u32 pass_index, u32 cull_instance_index_start,
                         u32 cull_instance_count)
    {
        Assert(node.mesh);

        const u32 index_count = static_cast<u32>(node.mesh->indexes.size());
        Assert(index_count % 3 == 0);

        CullPushConstants consts;
        consts.triangleCount = index_count / 3;
        consts.firstIndex = 0;
        consts.outputIndexOffset = pass_index * (DynamicIndexBufferSize / IndexSizeBytes);
        consts.firstInstance = cull_instance_index_start;

        CullCmd& command = cull_pass.cull_cmds.emplace_back();
        command.instanceCount = cull_instance_count;
        command.push_constants = consts;
    }
} // namespace

void prepare_scene(SceneGraph& scene, PreparedData& prepared)
{
    const Node& camera_node = scene.nodes[scene.camera.scene_node];

    // Main + culling pass
    const glm::mat4 main_camera_view_proj = scene.camera.projection_matrix * glm::mat4(camera_node.transform_matrix);

    prepared.draw_pass_params.view = camera_node.transform_matrix;
    prepared.draw_pass_params.proj = scene.camera.projection_matrix;
    prepared.draw_pass_params.view_proj = main_camera_view_proj;

    {
        const Node& light_node = scene.nodes[scene.lights.front().scene_node];

        const glm::vec3 light_position_ws =
            glm::inverse(glm::mat4(light_node.transform_matrix)) * glm::vec4(0.f, 0.f, 0.f, 1.0f);
        const glm::vec3 light_position_vs = camera_node.transform_matrix * glm::fvec4(light_position_ws, 1.f);

        prepared.draw_pass_params.point_light.position_vs = light_position_vs;
        prepared.draw_pass_params.point_light.intensity = 8.f;
        prepared.draw_pass_params.point_light.color = glm::fvec3(0.8f, 0.5f, 0.2f);
    }

    {
        CullPassData& cull_pass = prepared.cull_passes.emplace_back();
        const u32     pass_index = prepared.cull_passes.size() - 1;

        for (const auto& node : scene.nodes)
        {
            if (node.instance_id == InvalidMeshInstanceId)
                continue;

            // Assumption that our 3x3 submatrix is orthonormal (no skew/non-uniform scaling)
            // FIXME use 4x3 matrices directly
            const glm::mat4x3 modelView = glm::mat4(camera_node.transform_matrix) * glm::mat4(node.transform_matrix);

            DrawInstanceParams& draw_instance = prepared.draw_instance_params.emplace_back();
            draw_instance.model = node.transform_matrix, draw_instance.normal_ms_to_vs_matrix = glm::mat3(modelView);

            CullInstanceParams& cull_instance = prepared.cull_instance_params.emplace_back();
            const u32           cull_instance_index = prepared.cull_instance_params.size() - 1;

            cull_instance.ms_to_cs_matrix = main_camera_view_proj * glm::mat4(node.transform_matrix);
            cull_instance.instance_id = node.instance_id;

            insert_cull_cmd(cull_pass, node, pass_index, cull_instance_index, 1);
        }
    }

    // Shadow pass
    for (const auto& light : scene.lights)
    {
        const Node& light_node = scene.nodes[light.scene_node];

        CullPassData& cull_pass = prepared.cull_passes.emplace_back();
        const u32     pass_index = prepared.cull_passes.size() - 1;

        prepared.shadow_pass_params.dummy = glm::mat4(1.f);

        for (const auto& node : scene.nodes)
        {
            if (node.instance_id == InvalidMeshInstanceId)
                continue;

            const glm::mat4 light_view_proj_matrix = light.projection_matrix * glm::mat4(light_node.transform_matrix);

            ShadowMapInstanceParams& shadow_instance = prepared.shadow_instance_params.emplace_back();
            shadow_instance.ms_to_cs_matrix = light_view_proj_matrix * glm::mat4(node.transform_matrix);

            CullInstanceParams& cull_instance = prepared.cull_instance_params.emplace_back();
            const u32           cull_instance_index = prepared.cull_instance_params.size() - 1;

            cull_instance.ms_to_cs_matrix = shadow_instance.ms_to_cs_matrix;
            cull_instance.instance_id = node.instance_id;

            insert_cull_cmd(cull_pass, node, pass_index, cull_instance_index, 1);
        }
    }
}
} // namespace Reaper
