////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestTiledLighting.h"

#include "Geometry.h"

#include "mesh/ModelLoader.h"
#include "renderer/Mesh2.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/BackendResources.h"
#include "renderer/vulkan/MeshCache.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Reaper
{
void allocate_scene_resources(ReaperRoot& root, VulkanBackend& backend, std::span<ReaperGeometry> geometries)
{
    std::vector<Mesh>        meshes; // They don't need to persist any longer that that.
    std::vector<const char*> texture_filenames;

    // Used to write back the handles at the correct place
    std::vector<MeshHandle*>    mesh_indirections;    // FIXME
    std::vector<TextureHandle*> texture_indirections; // FIXME

    for (auto& geometry : geometries)
    {
        ReaperMaterial& material = geometry.material;
        Assert(material.albedo.source != EmptySource);
        texture_filenames.push_back(material.albedo.source);
        texture_indirections.push_back(&material.albedo.handle);

        ReaperMesh& mesh = geometry.mesh;
        Assert(mesh.source != EmptySource);
        meshes.push_back(ModelLoader::loadOBJ(mesh.source));
        mesh_indirections.push_back(&mesh.handle);
    }

    std::vector<MeshHandle> mesh_handles;
    mesh_handles.resize(meshes.size());
    load_meshes(backend, backend.resources->mesh_cache, meshes, mesh_handles);

    // Fixup pointers
    Assert(mesh_indirections.size() == meshes.size());
    for (u32 i = 0; i < mesh_indirections.size(); i++)
    {
        MeshHandle* const target = mesh_indirections[i];
        *target = mesh_handles[i];
    }

    std::vector<TextureHandle>& texture_handles = backend.resources->material_resources.texture_handles;
    texture_handles.resize(texture_filenames.size());
    load_textures(root, backend, backend.resources->material_resources, texture_filenames, texture_handles);

    // Fixup pointers
    Assert(texture_indirections.size() == texture_handles.size());
    for (u32 i = 0; i < texture_indirections.size(); i++)
    {
        TextureHandle* const target = texture_indirections[i];
        *target = texture_handles[i];
    }
}

SceneGraph create_static_test_scene(ReaperRoot& root, VulkanBackend& backend)
{
    std::vector<ReaperGeometry> geometries;
    ReaperGeometry&             asteroid_geometry = geometries.emplace_back();
    asteroid_geometry.material.albedo.source = "res/texture/default.dds";
    asteroid_geometry.mesh.source = "res/model/asteroid.obj";

    allocate_scene_resources(root, backend, geometries);

    SceneGraph scene;
    scene.camera_node = create_scene_node(scene, glm::translate(glm::mat4(1.0f), glm::vec3(-10.f, 3.f, 3.f)));

    // FIXME
    const SceneMaterialHandle material_handle = static_cast<SceneMaterialHandle>(scene.scene_materials.size());

    scene.scene_materials.emplace_back(SceneMaterial{
        .base_color_texture = asteroid_geometry.material.albedo.handle,
        .metal_roughness_texture = InvalidTextureHandle,
        .normal_map_texture = InvalidTextureHandle,
    });

    constexpr i32 asteroid_count = 4;

    // Place static track
    for (i32 i = 0; i < asteroid_count; i++)
    {
        for (i32 j = 0; j < asteroid_count; j++)
        {
            for (i32 k = 0; k < asteroid_count; k++)
            {
                glm::fvec3 position_ws(static_cast<float>(i * 2), static_cast<float>(j * 2), static_cast<float>(k * 2));
                glm::fmat4x3 transform = glm::translate(glm::mat4(1.0f), position_ws);

                Reaper::SceneMesh& scene_mesh = scene.scene_meshes.emplace_back();
                scene_mesh.scene_node = create_scene_node(scene, transform);
                scene_mesh.mesh_handle = asteroid_geometry.mesh.handle;
                scene_mesh.material_handle = material_handle;
            }
        }
    }

    const glm::vec3    light_target_ws = glm::vec3(0.f, 0.f, 0.f);
    const glm::vec3    up_ws = glm::vec3(0.f, 1.f, 0.f);
    const glm::vec3    light_position_ws = glm::vec3(-4.f, -4.f, -4.f);
    const glm::fmat4x3 light_transform = glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

    SceneLight& light = scene.scene_lights.emplace_back();
    light.color = glm::fvec3(1.f, 1.f, 1.f);
    light.intensity = 60.f;
    light.radius = 10.f;
    light.scene_node = create_scene_node(scene, light_transform);
    light.shadow_map_size = glm::uvec2(1024, 1024);

    return scene;
}

SceneGraph create_test_scene_tiled_lighting(ReaperRoot& root, VulkanBackend& backend)
{
    std::vector<ReaperGeometry> geometries;
    ReaperGeometry&             asteroid_geometry = geometries.emplace_back();
    asteroid_geometry.material.albedo.source = "res/texture/default.dds";
    asteroid_geometry.mesh.source = "res/model/quad.obj";

    allocate_scene_resources(root, backend, geometries);

    SceneGraph scene;

    // FIXME
    const SceneMaterialHandle material_handle = static_cast<SceneMaterialHandle>(scene.scene_materials.size());

    scene.scene_materials.emplace_back(SceneMaterial{
        .base_color_texture = asteroid_geometry.material.albedo.handle,
        .metal_roughness_texture = InvalidTextureHandle,
        .normal_map_texture = InvalidTextureHandle,
    });

    const glm::vec3    up_ws = glm::vec3(0.f, 1.f, 0.f);
    const glm::fvec3   camera_position = glm::vec3(5.f, 5.f, 5.f);
    const glm::fvec3   camera_local_target = glm::vec3(0.f, 0.f, 0.f);
    const glm::fmat4x3 camera_local_transform = glm::inverse(glm::lookAt(camera_position, camera_local_target, up_ws));

    scene.camera_node = create_scene_node(scene, camera_local_transform);

    const glm::fmat4x3 flat_quad_transform =
        glm::rotate(glm::identity<glm::fmat4>(), glm::pi<float>() * -0.5f, glm::fvec3(1.f, 0.f, 0.f));
    const glm::fvec3   flat_quad_scale = glm::fvec3(4.f, 4.f, 4.f);
    const glm::fmat4x3 transform =
        glm::fmat4(flat_quad_transform) * glm::scale(glm::identity<glm::fmat4>(), flat_quad_scale);

    SceneMesh& scene_mesh = scene.scene_meshes.emplace_back();
    scene_mesh.scene_node = create_scene_node(scene, transform);
    scene_mesh.mesh_handle = asteroid_geometry.mesh.handle;
    scene_mesh.material_handle = material_handle;

    SceneLight& light = scene.scene_lights.emplace_back();
    light.color = glm::fvec3(1.f, 0.f, 1.f);
    light.intensity = 3.f;
    light.radius = 2.f;
    light.scene_node = create_scene_node(scene, glm::translate(glm::mat4(1.0f), glm::fvec3(0.f, 1.f, 0.f)));
    light.shadow_map_size = glm::uvec2(0, 0);

    SceneLight& light2 = scene.scene_lights.emplace_back();
    light2.color = glm::fvec3(1.f, 0.f, 0.f);
    light2.intensity = 3.f;
    light2.radius = 2.f;
    light2.scene_node = create_scene_node(scene, glm::translate(glm::mat4(1.0f), glm::fvec3(2.f, 0.1f, 2.f)));
    light2.shadow_map_size = glm::uvec2(0, 0);

    return scene;
}
} // namespace Reaper
