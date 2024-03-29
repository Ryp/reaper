////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PrepareBuckets.h"

#include "Camera.h"

#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/renderpass/ShadowConstants.h"

#include "profiling/Scope.h"

#include "math/Constants.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace Reaper
{
namespace
{
    float note(float semitone_offset, float base_freq = 440.f)
    {
        return base_freq * powf(2.f, semitone_offset / 12.f);
    }

    glm::fmat4 apply_reverse_z_fixup(glm::fmat4 projection_matrix, bool reverse_z)
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

    glm::fmat4 build_perspective_matrix(float near_plane, float far_plane, float aspect_ratio, float fov_radian)
    {
        glm::fmat4 projection = glm::perspective(fov_radian, aspect_ratio, near_plane, far_plane);

        // Flip viewport Y
        projection[1] = -projection[1];

        return projection;
    }

    glm::fmat4 default_light_projection_matrix()
    {
        return apply_reverse_z_fixup(build_perspective_matrix(0.1f, 100.f, 1.f, glm::pi<float>() * 0.25f),
                                     ShadowUseReverseZ);
    }
} // namespace

SceneNode* create_scene_node(SceneGraph& scene, glm::mat4x3 transform_matrix, SceneNode* parent_node)
{
    SceneNode* node = new SceneNode;
    node->transform_matrix = transform_matrix;
    node->parent = parent_node;

    static_cast<void>(scene); // FIXME use for some kind of pool allocator?

    return node;
}

void destroy_scene_node(SceneGraph& scene, SceneNode* node)
{
    static_cast<void>(scene); // FIXME use for some kind of pool allocator?

    delete node;
}

glm::fmat4x3 get_scene_node_transform_slow(SceneNode* node)
{
    Assert(node != nullptr);

    glm::fmat4x4 accum = node->transform_matrix;

    while (node->parent != nullptr)
    {
        accum = glm::fmat4(node->parent->transform_matrix) * accum;
        node = node->parent;
    }

    return accum;
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
                   const RendererPerspectiveCamera& main_camera, u32 current_audio_frame)
{
    REAPER_PROFILE_SCOPE_FUNC();

    // Shadow pass
    for (const auto& light : scene.scene_lights)
    {
        if (light.shadow_map_size == glm::uvec2(0, 0))
            continue;

        ShadowPassData& shadow_pass = prepared.shadow_passes.emplace_back();
        shadow_pass.pass_index = static_cast<u32>(prepared.shadow_passes.size() - 1);

        shadow_pass.instance_offset = static_cast<u32>(prepared.shadow_instance_params.size());

        CullPassData& cull_pass = prepared.cull_passes.emplace_back();
        cull_pass.pass_index = static_cast<u32>(prepared.cull_passes.size() - 1);
        cull_pass.output_size_ts = glm::fvec2(light.shadow_map_size);
        cull_pass.main_pass = false;

        shadow_pass.culling_pass_index = cull_pass.pass_index;
        shadow_pass.shadow_map_size = light.shadow_map_size;

        const glm::fmat4x3 light_transform = get_scene_node_transform_slow(light.scene_node);
        const glm::fmat4x3 light_transform_inv = glm::inverse(glm::fmat4(light_transform));
        const glm::fmat4   light_projection_matrix = default_light_projection_matrix();
        const glm::fmat4   light_view_proj_matrix = light_projection_matrix * glm::mat4(light_transform_inv);

        for (u32 i = 0; i < scene.scene_meshes.size(); i++)
        {
            const SceneMesh&   scene_mesh = scene.scene_meshes[i];
            const glm::fmat4x3 mesh_transform = get_scene_node_transform_slow(scene_mesh.scene_node);

            ShadowMapInstanceParams& shadow_instance = prepared.shadow_instance_params.emplace_back();
            shadow_instance.ms_to_cs_matrix = light_view_proj_matrix * glm::mat4(mesh_transform);

            const u32               cull_instance_index = static_cast<u32>(prepared.cull_mesh_instance_params.size());
            CullMeshInstanceParams& cull_instance = prepared.cull_mesh_instance_params.emplace_back();

            cull_instance.ms_to_cs_matrix = shadow_instance.ms_to_cs_matrix;

            const glm::mat4x3 ms_to_vs_matrix = glm::mat4(light_transform_inv) * glm::mat4(mesh_transform);
            const glm::mat4x3 vs_to_ms_matrix = glm::inverse(glm::mat4(ms_to_vs_matrix));

            cull_instance.vs_to_ms_matrix_translate = vs_to_ms_matrix * glm::vec4(0.f, 0.f, 0.f, 1.f);
            cull_instance.instance_id = i;

            const Mesh2&     mesh2 = mesh_cache.mesh2_instances[scene_mesh.mesh_handle];
            const MeshAlloc& mesh_alloc = mesh2.lods_allocs[0];

            insert_cull_command(cull_pass, mesh_alloc, cull_instance_index, 1);
        }

        // Count instances we just inserted
        const u32 shadow_total_instance_count = static_cast<u32>(prepared.shadow_instance_params.size());
        shadow_pass.instance_count = shadow_total_instance_count - shadow_pass.instance_offset;
    }

    // Main + culling pass
    prepared.forward_pass_constants.ws_to_vs_matrix = main_camera.ws_to_vs_matrix;
    prepared.forward_pass_constants.ws_to_cs_matrix = main_camera.ws_to_cs_matrix;
    prepared.forward_pass_constants.point_light_count = static_cast<u32>(scene.scene_lights.size());

    prepared.tiled_light_constants.cs_to_vs = main_camera.perspective_projection.cs_to_vs_matrix;
    prepared.tiled_light_constants.vs_to_ws = main_camera.vs_to_ws_matrix;

    for (u32 scene_light_index = 0; scene_light_index < scene.scene_lights.size(); scene_light_index++)
    {
        const SceneLight&  light = scene.scene_lights[scene_light_index];
        const glm::fmat4x3 light_transform = get_scene_node_transform_slow(light.scene_node);
        const glm::fmat4x3 light_transform_inv = glm::inverse(glm::fmat4(light_transform));

        const glm::vec3 light_position_ws = light_transform * glm::vec4(0.f, 0.f, 0.f, 1.0f);
        const glm::vec3 light_position_vs = main_camera.ws_to_vs_matrix * glm::fvec4(light_position_ws, 1.f);

        const glm::fmat4 light_projection_matrix = default_light_projection_matrix();
        const glm::fmat4 light_view_proj_matrix = light_projection_matrix * glm::fmat4(light_transform_inv);

        PointLightProperties& point_light = prepared.point_lights.emplace_back();
        point_light.light_ws_to_cs = light_view_proj_matrix;
        point_light.position_vs = light_position_vs;
        point_light.intensity = light.intensity;
        point_light.color = light.color;
        point_light.radius_sq = light.radius * light.radius;
        point_light.shadow_map_index = InvalidShadowMapIndex;

        if (light.shadow_map_size != glm::uvec2(0, 0))
        {
            point_light.shadow_map_index = scene_light_index; // FIXME
        }
    }

    {
        CullPassData& cull_pass = prepared.cull_passes.emplace_back();
        cull_pass.pass_index = static_cast<u32>(prepared.cull_passes.size() - 1);
        cull_pass.output_size_ts = glm::fvec2(main_camera.viewport.extent);
        cull_pass.main_pass = true;

        prepared.main_culling_pass_index = cull_pass.pass_index;

        for (u32 i = 0; i < scene.scene_meshes.size(); i++)
        {
            const SceneMesh&     scene_mesh = scene.scene_meshes[i];
            const SceneMaterial& scene_material = scene.scene_materials[scene_mesh.material_handle];
            const glm::fmat4x3   mesh_transform = get_scene_node_transform_slow(scene_mesh.scene_node);

            // Assumption that our 3x3 submatrix is orthonormal (no skew/non-uniform scaling)
            // FIXME use 4x3 matrices directly
            const glm::mat4x3 ms_to_vs_matrix = glm::mat4(main_camera.ws_to_vs_matrix) * glm::mat4(mesh_transform);

            MeshInstance& mesh_instance = prepared.mesh_instances.emplace_back();
            mesh_instance.ms_to_cs_matrix = main_camera.ws_to_cs_matrix * glm::mat4(mesh_transform);
            mesh_instance.ms_to_ws_matrix = mesh_transform;
            mesh_instance.normal_ms_to_vs_matrix = glm::mat3(ms_to_vs_matrix);
            mesh_instance.albedo_texture_index = scene_material.base_color_texture;
            mesh_instance.roughness_texture_index = scene_material.metal_roughness_texture;
            mesh_instance.normal_texture_index = scene_material.normal_map_texture;
            mesh_instance.ao_texture_index = scene_material.ao_texture;

            const u32               cull_instance_index = static_cast<u32>(prepared.cull_mesh_instance_params.size());
            CullMeshInstanceParams& cull_instance = prepared.cull_mesh_instance_params.emplace_back();

            cull_instance.ms_to_cs_matrix = mesh_instance.ms_to_cs_matrix;

            const glm::mat4x3 vs_to_ms_matrix = glm::inverse(glm::mat4(ms_to_vs_matrix));
            cull_instance.vs_to_ms_matrix_translate = vs_to_ms_matrix * glm::vec4(0.f, 0.f, 0.f, 1.f);
            cull_instance.instance_id = i;

            const Mesh2&     mesh2 = mesh_cache.mesh2_instances[scene_mesh.mesh_handle];
            const MeshAlloc& mesh_alloc = mesh2.lods_allocs[0];

            insert_cull_command(cull_pass, mesh_alloc, cull_instance_index, 1);
        }
    }

    // Audio pass
    prepared.audio_push_constants.start_sample = current_audio_frame;
    for (u32 i = 0; i < OscillatorCount; i++)
    {
        OscillatorInstance& instance = prepared.audio_instance_params.emplace_back();

        instance.frequency = note(0.f + static_cast<float>(i) * 5.f);
        instance.pan = -0.60f + static_cast<float>(i) * 0.4f;
    }
}
} // namespace Reaper
