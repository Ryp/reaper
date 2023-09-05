////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/glm.hpp>

#include "renderer/Mesh2.h" // FIXME
#include "renderer/RendererExport.h"
#include "renderer/ResourceHandle.h"

#include <span>
#include <vector>

#include "renderer/shader/forward.share.hlsl"
#include "renderer/shader/lighting.share.hlsl"
#include "renderer/shader/mesh_instance.share.hlsl"
#include "renderer/shader/meshlet/meshlet_culling.share.hlsl"
#include "renderer/shader/shadow/shadow_map_pass.share.hlsl"
#include "renderer/shader/sound/sound.share.hlsl"
#include "renderer/shader/tiled_lighting/tiled_lighting.share.hlsl"

#include <core/Types.h>

namespace Reaper
{
struct SceneNode
{
    glm::fmat4x3 transform_matrix; // Local space to parent space
    SceneNode*   parent = nullptr; // If no parent, parent space is world space
};

struct SceneMaterial
{
    TextureHandle base_color_texture;
    TextureHandle metal_roughness_texture;
    TextureHandle normal_map_texture;
    TextureHandle ao_texture;
};

enum SceneMaterialHandle : u32
{
};

static constexpr SceneMaterialHandle InvalidSceneMaterialHandle = SceneMaterialHandle(0xFFFFFFFF);

struct SceneMesh
{
    SceneNode*          scene_node;
    MeshHandle          mesh_handle;
    SceneMaterialHandle material_handle;
};

struct SceneLight
{
    glm::mat4  projection_matrix;
    glm::vec3  color;
    float      intensity;
    float      radius;
    SceneNode* scene_node;
    glm::uvec2 shadow_map_size; // Set to zero to disable shadow
};

struct SceneGraph
{
    SceneNode*                 camera_node;
    std::vector<SceneMesh>     scene_meshes;
    std::vector<SceneMaterial> scene_materials;
    std::vector<SceneLight>    scene_lights;
};

inline SceneMaterialHandle alloc_scene_material(SceneGraph& scene)
{
    const u32 old_size = scene.scene_materials.size();
    scene.scene_materials.resize(old_size + 1);

    return SceneMaterialHandle(old_size);
}

inline HandleSpan<SceneMaterialHandle> alloc_scene_materials(SceneGraph& scene, u32 count)
{
    const u32 old_size = scene.scene_materials.size();

    scene.scene_materials.resize(old_size + count);

    return HandleSpan<SceneMaterialHandle>{
        .offset = old_size,
        .count = count,
    };
}

REAPER_RENDERER_API SceneNode* create_scene_node(SceneGraph& scene, glm::mat4x3 transform_matrix,
                                                 SceneNode* parent_node = nullptr);

REAPER_RENDERER_API void destroy_scene_node(SceneGraph& scene, SceneNode* node);

// FIXME Support proper parenting with caching and disallow cycles!
REAPER_RENDERER_API glm::fmat4x3 get_scene_node_transform_slow(SceneNode* node);

struct CullCmd
{
    MeshAlloc                mesh_alloc;
    CullMeshletPushConstants push_constants;
    u32                      instance_count;
};

struct CullPassData
{
    u32        pass_index;
    glm::fvec2 output_size_ts;
    bool       main_pass;

    std::vector<CullCmd> cull_commands;
};

struct ShadowPassData
{
    u32        pass_index;
    u32        instance_offset;
    u32        instance_count;
    u32        culling_pass_index;
    glm::uvec2 shadow_map_size;
};

struct PreparedData
{
    std::vector<CullPassData>           cull_passes;
    std::vector<CullMeshInstanceParams> cull_mesh_instance_params;

    std::vector<MeshInstance> mesh_instances;

    u32               main_culling_pass_index;
    ForwardPassParams forward_pass_constants;

    std::vector<PointLightProperties> point_lights;
    TiledLightingConstants            tiled_light_constants;

    std::vector<ShadowPassData>          shadow_passes;
    std::vector<ShadowMapInstanceParams> shadow_instance_params;

    SoundPushConstants              audio_push_constants;
    std::vector<OscillatorInstance> audio_instance_params;
};

struct MeshCache;
struct RendererPerspectiveCamera;

void prepare_scene(const SceneGraph& scene, PreparedData& prepared, const MeshCache& mesh_cache,
                   const RendererPerspectiveCamera& main_camera, u32 current_audio_frame);
} // namespace Reaper
