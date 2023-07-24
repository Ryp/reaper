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

#include <nonstd/span.hpp>
#include <vector>

#include "renderer/shader/forward.share.hlsl"
#include "renderer/shader/lighting.share.hlsl"
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

struct SceneMesh
{
    SceneNode*    scene_node;
    MeshHandle    mesh_handle;
    TextureHandle texture_handle;
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
    SceneNode*              camera_node;
    std::vector<SceneMesh>  scene_meshes;
    std::vector<SceneLight> scene_lights;
};

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

    u32                                forward_culling_pass_index;
    ForwardPassParams                  forward_pass_constants;
    std::vector<ForwardInstanceParams> forward_instances;

    std::vector<PointLightProperties> point_lights;
    TiledLightingConstants            tiled_light_constants;

    std::vector<ShadowPassData>          shadow_passes;
    std::vector<ShadowMapPassParams>     shadow_pass_params;
    std::vector<ShadowMapInstanceParams> shadow_instance_params;

    SoundPushConstants              audio_push_constants;
    std::vector<OscillatorInstance> audio_instance_params;
};

struct MeshCache;
struct RendererPerspectiveCamera;

void prepare_scene(const SceneGraph& scene, PreparedData& prepared, const MeshCache& mesh_cache,
                   const RendererPerspectiveCamera& main_camera, u32 current_audio_frame);
} // namespace Reaper
