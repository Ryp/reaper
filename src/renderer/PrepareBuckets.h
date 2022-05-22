////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include <glm/glm.hpp>

#include "renderer/Mesh2.h" // FIXME
#include "renderer/ResourceHandle.h"

#include <nonstd/span.hpp>
#include <vector>

#include "renderer/shader/share/draw.hlsl"
#include "renderer/shader/share/meshlet_culling.hlsl"
#include "renderer/shader/share/shadow_map_pass.hlsl"
#include "renderer/shader/share/sound.hlsl"
#include "renderer/shader/share/swapchain.hlsl"

namespace Reaper
{
struct Node
{
    glm::fmat4x3 transform_matrix;
};

struct SceneCamera
{
    u32 scene_node;
};

struct SceneMesh
{
    u32           node_index;
    MeshHandle    mesh_handle;
    TextureHandle texture_handle;
};

struct SceneLight
{
    glm::mat4  projection_matrix;
    glm::vec3  color;
    float      intensity;
    u32        scene_node;
    glm::uvec2 shadow_map_size;
};

struct SceneGraph
{
    std::vector<Node> nodes;

    SceneCamera             camera;
    std::vector<SceneMesh>  meshes;
    std::vector<SceneLight> lights;
};

REAPER_RENDERER_API u32 insert_scene_node(SceneGraph& scene, glm::mat4x3 transform_matrix);
REAPER_RENDERER_API u32 insert_scene_mesh(SceneGraph& scene, SceneMesh scene_mesh);
REAPER_RENDERER_API u32 insert_scene_light(SceneGraph& scene, SceneLight scene_light);

REAPER_RENDERER_API glm::fmat4 build_perspective_matrix(float near_plane, float far_plane, float aspect_ratio,
                                                        float fov_radian);

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

    u32                             draw_culling_pass_index;
    DrawPassParams                  draw_pass_params;
    std::vector<DrawInstanceParams> draw_instance_params;

    std::vector<ShadowPassData>          shadow_passes;
    std::vector<ShadowMapPassParams>     shadow_pass_params;
    std::vector<ShadowMapInstanceParams> shadow_instance_params;

    SoundPushConstants              audio_push_constants;
    std::vector<OscillatorInstance> audio_instance_params;

    SwapchainPassParams swapchain_pass_params;
};

struct MeshCache;

void prepare_scene(const SceneGraph& scene, PreparedData& prepared, const MeshCache& mesh_cache,
                   glm::uvec2 viewport_extent, u32 current_audio_frame);
} // namespace Reaper
