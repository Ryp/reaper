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

#include "renderer/shader/share/forward.hlsl"
#include "renderer/shader/share/lighting.hlsl"
#include "renderer/shader/share/meshlet_culling.hlsl"
#include "renderer/shader/share/shadow_map_pass.hlsl"
#include "renderer/shader/share/sound.hlsl"
#include "renderer/shader/share/swapchain.hlsl"

namespace Reaper
{
static constexpr u32 InvalidNodeIndex = u32(-1);
struct Node
{
    glm::fmat4x3 transform_matrix;
    u32          parent_scene_node = InvalidNodeIndex;
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
    glm::uvec2 shadow_map_size; // Set to zero to disable shadow
};

struct SceneGraph
{
    std::vector<Node> nodes;

    SceneCamera             camera;
    std::vector<SceneMesh>  meshes;
    std::vector<SceneLight> lights;
};

REAPER_RENDERER_API u32 insert_scene_node(SceneGraph& scene, glm::mat4x3 transform_matrix,
                                          u32 parent_node_index = InvalidNodeIndex);
REAPER_RENDERER_API u32 insert_scene_mesh(SceneGraph& scene, SceneMesh scene_mesh);
REAPER_RENDERER_API u32 insert_scene_light(SceneGraph& scene, SceneLight scene_light);

// FIXME Support proper parenting with caching and disallow cycles!
REAPER_RENDERER_API glm::fmat4x3 get_scene_node_transform_slow(const SceneGraph& scene, u32 node_index);

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
