////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PrepareBuckets.h"

#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/renderpass/ShadowConstants.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "math/Constants.h"

namespace Reaper
{
constexpr bool UseReverseZ = true;
constexpr u32  InvalidMeshInstanceId = -1;

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

    float note(float semitone_offset, float base_freq = 440.f) { return base_freq * powf(2.f, semitone_offset / 12.f); }
} // namespace

void build_scene_graph(SceneGraph& scene)
{
    // Add lights
    {
        const glm::mat4 light_projection_matrix = build_perspective_matrix(0.1f, 100.f, 1.f, glm::pi<float>() * 0.25f);
        const glm::vec3 light_target_ws = glm::vec3(0.f, 0.f, 0.f);
        const glm::vec3 up_ws = glm::vec3(0.f, 1.f, 0.f);

        // Add 1st light
        {
            Node&     light_node = scene.nodes.emplace_back();
            const u32 light_node_index = scene.nodes.size() - 1;

            const glm::vec3 light_position_ws = glm::vec3(-2.f, 2.f, 2.f);

            light_node.instance_id = InvalidMeshInstanceId;
            light_node.transform_matrix = glm::lookAt(light_position_ws, light_target_ws, up_ws);

            Light& light = scene.lights.emplace_back();
            light.color = glm::fvec3(0.03f, 0.21f, 0.61f);
            light.intensity = 6.f;
            light.scene_node = light_node_index;
            light.projection_matrix = light_projection_matrix;
            light.shadow_map_size = glm::uvec2(1024, 1024);
        }

        // Add 2nd light
        {
            Node&     light_node = scene.nodes.emplace_back();
            const u32 light_node_index = scene.nodes.size() - 1;

            const glm::vec3 light_position_ws = glm::vec3(-2.f, -2.f, -2.f);

            light_node.instance_id = InvalidMeshInstanceId;
            light_node.transform_matrix = glm::lookAt(light_position_ws, light_target_ws, up_ws);

            Light& light = scene.lights.emplace_back();
            light.color = glm::fvec3(0.61f, 0.21f, 0.03f);
            light.intensity = 6.f;
            light.scene_node = light_node_index;
            light.projection_matrix = light_projection_matrix;
            light.shadow_map_size = glm::uvec2(512, 512);
        }

        // Add 3rd light
        {
            Node&     light_node = scene.nodes.emplace_back();
            const u32 light_node_index = scene.nodes.size() - 1;

            const glm::vec3 light_position_ws = glm::vec3(0.f, -2.f, 2.f);

            light_node.instance_id = InvalidMeshInstanceId;
            light_node.transform_matrix = glm::lookAt(light_position_ws, light_target_ws, up_ws);

            Light& light = scene.lights.emplace_back();
            light.color = glm::fvec3(0.03f, 0.8f, 0.21f);
            light.intensity = 6.f;
            light.scene_node = light_node_index;
            light.projection_matrix = light_projection_matrix;
            light.shadow_map_size = glm::uvec2(256, 256);
        }
    }

    // Add camera
    {
        Node&     camera_node = scene.nodes.emplace_back();
        const u32 camera_node_index = scene.nodes.size() - 1;

        camera_node.instance_id = InvalidMeshInstanceId;

        scene.camera.scene_node = camera_node_index;
    }
}

namespace
{
    void insert_cull_command(CullPassData& cull_pass, const MeshAlloc& mesh_alloc, u32 cull_instance_index_start,
                             u32 cull_instance_count)
    {
        const u32 index_count = static_cast<u32>(mesh_alloc.index_count);
        Assert(index_count % 3 == 0);

        CullMeshletPushConstants consts;
        consts.meshlet_offset = mesh_alloc.meshlet_offset;
        consts.meshlet_count = mesh_alloc.meshlet_count;
        consts.first_index = mesh_alloc.index_offset;
        consts.first_vertex = mesh_alloc.position_offset;
        consts.cull_instance_offset = cull_instance_index_start;

        CullCmd& command = cull_pass.cull_commands.emplace_back();
        command.instance_count = cull_instance_count;
        command.push_constants = consts;
    }
} // namespace

void prepare_scene(const SceneGraph& scene, PreparedData& prepared, const MeshCache& mesh_cache,
                   glm::uvec2 viewport_extent, u32 current_audio_frame)
{
    // Shadow pass
    for (const auto& light : scene.lights)
    {
        ShadowPassData& shadow_pass = prepared.shadow_passes.emplace_back();
        shadow_pass.pass_index = prepared.shadow_passes.size() - 1;

        shadow_pass.instance_offset = prepared.shadow_instance_params.size();

        CullPassData& cull_pass = prepared.cull_passes.emplace_back();
        cull_pass.pass_index = prepared.cull_passes.size() - 1;
        cull_pass.output_size_ts = glm::fvec2(light.shadow_map_size);

        shadow_pass.culling_pass_index = cull_pass.pass_index;
        shadow_pass.shadow_map_size = light.shadow_map_size;

        ShadowMapPassParams& shadow_pass_params = prepared.shadow_pass_params.emplace_back();
        shadow_pass_params.dummy = glm::mat4(1.f);

        const Node& light_node = scene.nodes[light.scene_node];

        for (const auto& node : scene.nodes)
        {
            if (node.instance_id == InvalidMeshInstanceId)
                continue;

            const glm::mat4 light_view_proj_matrix = light.projection_matrix * glm::mat4(light_node.transform_matrix);

            ShadowMapInstanceParams& shadow_instance = prepared.shadow_instance_params.emplace_back();
            shadow_instance.ms_to_cs_matrix = light_view_proj_matrix * glm::mat4(node.transform_matrix);

            const u32               cull_instance_index = prepared.cull_mesh_instance_params.size();
            CullMeshInstanceParams& cull_instance = prepared.cull_mesh_instance_params.emplace_back();

            cull_instance.ms_to_cs_matrix = shadow_instance.ms_to_cs_matrix;
            cull_instance.instance_id = node.instance_id;

            const Mesh2&     mesh2 = mesh_cache.mesh2_instances[node.mesh_handle];
            const MeshAlloc& mesh_alloc = mesh2.lods_allocs[0];

            insert_cull_command(cull_pass, mesh_alloc, cull_instance_index, 1);
        }

        // Count instances we just inserted
        const u32 shadow_total_instance_count = prepared.shadow_instance_params.size();
        shadow_pass.instance_count = shadow_total_instance_count - shadow_pass.instance_offset;
    }

    const float near_plane_distance = 0.1f;
    const float far_plane_distance = 100.f;
    const float fov_radian = glm::pi<float>() * 0.25f;
    const float aspect_ratio = static_cast<float>(viewport_extent.x) / static_cast<float>(viewport_extent.y);

    glm::mat4 camera_projection_matrix =
        build_perspective_matrix(near_plane_distance, far_plane_distance, aspect_ratio, fov_radian);

    const Node& camera_node = scene.nodes[scene.camera.scene_node];

    // Main + culling pass
    const glm::mat4 main_camera_view_proj = camera_projection_matrix * glm::mat4(camera_node.transform_matrix);

    prepared.draw_pass_params.ws_to_vs_matrix = camera_node.transform_matrix;
    prepared.draw_pass_params.vs_to_cs_matrix = camera_projection_matrix;
    prepared.draw_pass_params.ws_to_cs_matrix = main_camera_view_proj;

    Assert(scene.lights.size() == PointLightCount);
    for (u32 i = 0; i < PointLightCount; i++)
    {
        const Light& light = scene.lights[i];
        const Node&  light_node = scene.nodes[light.scene_node];

        const glm::vec3 light_position_ws =
            glm::inverse(glm::mat4(light_node.transform_matrix)) * glm::vec4(0.f, 0.f, 0.f, 1.0f);
        const glm::vec3 light_position_vs = camera_node.transform_matrix * glm::fvec4(light_position_ws, 1.f);

        const glm::mat4 light_view_proj_matrix = light.projection_matrix * glm::mat4(light_node.transform_matrix);

        prepared.draw_pass_params.point_light[i].light_ws_to_cs = light_view_proj_matrix;
        prepared.draw_pass_params.point_light[i].position_vs = light_position_vs;
        prepared.draw_pass_params.point_light[i].intensity = light.intensity;
        prepared.draw_pass_params.point_light[i].color = light.color;
        prepared.draw_pass_params.point_light[i].shadow_map_index = i; // FIXME
    }

    {
        CullPassData& cull_pass = prepared.cull_passes.emplace_back();
        cull_pass.pass_index = prepared.cull_passes.size() - 1;
        cull_pass.output_size_ts = glm::fvec2(viewport_extent);

        prepared.draw_culling_pass_index = cull_pass.pass_index;

        for (const auto& node : scene.nodes)
        {
            if (node.instance_id == InvalidMeshInstanceId)
                continue;

            // Assumption that our 3x3 submatrix is orthonormal (no skew/non-uniform scaling)
            // FIXME use 4x3 matrices directly
            const glm::mat4x3 modelView = glm::mat4(camera_node.transform_matrix) * glm::mat4(node.transform_matrix);

            DrawInstanceParams& draw_instance = prepared.draw_instance_params.emplace_back();
            draw_instance.ms_to_ws_matrix = node.transform_matrix;
            draw_instance.normal_ms_to_vs_matrix = glm::mat3(modelView);
            draw_instance.texture_index = node.texture_handle;

            const u32               cull_instance_index = prepared.cull_mesh_instance_params.size();
            CullMeshInstanceParams& cull_instance = prepared.cull_mesh_instance_params.emplace_back();

            cull_instance.ms_to_cs_matrix = main_camera_view_proj * glm::mat4(node.transform_matrix);
            cull_instance.instance_id = node.instance_id;

            const Mesh2&     mesh2 = mesh_cache.mesh2_instances[node.mesh_handle];
            const MeshAlloc& mesh_alloc = mesh2.lods_allocs[0];

            insert_cull_command(cull_pass, mesh_alloc, cull_instance_index, 1);
        }
    }

    // Swapchain pass
    {
        SwapchainPassParams params = {};

        prepared.swapchain_pass_params = params;
    }

    // Audio pass
    prepared.audio_push_constants.start_sample = current_audio_frame;
    for (u32 i = 0; i < OscillatorCount; i++)
    {
        OscillatorInstance& instance = prepared.audio_instance_params.emplace_back();

        instance.frequency = note(0 + i * 5);
        instance.pan = -0.60f + i * 0.4f;
    }
}
} // namespace Reaper
