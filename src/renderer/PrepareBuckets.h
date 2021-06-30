////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include <glm/glm.hpp>

#include "renderer/ResourceHandle.h"

#include <nonstd/span.hpp>
#include <vector>

#include "renderer/shader/share/compaction.hlsl"
#include "renderer/shader/share/culling.hlsl"
#include "renderer/shader/share/draw.hlsl"
#include "renderer/shader/share/shadow_map_pass.hlsl"
#include "renderer/shader/share/swapchain.hlsl"

namespace Reaper
{
struct Light
{
    glm::mat4  projection_matrix;
    glm::vec3  color;
    float      intensity;
    u32        scene_node;
    glm::uvec2 shadow_map_size;
};

struct Node
{
    glm::mat4x3   transform_matrix;
    u32           instance_id;
    MeshHandle    mesh_handle;
    TextureHandle texture_handle;
};

struct SceneCamera
{
    glm::mat4  projection_matrix;
    glm::uvec2 viewport_extent;
    u32        scene_node;
};

struct SceneGraph
{
    std::vector<Node> nodes;

    std::vector<Light> lights;
    SceneCamera        camera;
};

void build_scene_graph(SceneGraph& scene, const nonstd::span<MeshHandle> mesh_handles,
                       const nonstd::span<TextureHandle> texture_handles);
void update_scene_graph(SceneGraph& scene, float time_ms, glm::uvec2 viewport_extent, const glm::mat4x3& view_matrix);

struct CullCmd
{
    u32               instanceCount;
    CullPushConstants push_constants;
};

struct CullMeshletCmd
{
    u32               meshlet_offset;
    u32               meshlet_count;
    u32               mesh_instance_count;
    CullPushConstants push_constants;
};

struct CullPassData
{
    u32 pass_index;

    std::vector<CullCmd>        cull_cmds;
    std::vector<CullMeshletCmd> cull_meshlet_cmds;
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
    std::vector<CullPassData>              cull_passes;
    std::vector<CullPassParams>            cull_pass_params;
    std::vector<CullMeshInstanceParams>    cull_mesh_instance_params;
    std::vector<CullMeshletInstanceParams> cull_meshlet_instance_params;

    u32                             draw_culling_pass_index;
    DrawPassParams                  draw_pass_params;
    std::vector<DrawInstanceParams> draw_instance_params;

    std::vector<ShadowPassData>          shadow_passes;
    std::vector<ShadowMapPassParams>     shadow_pass_params;
    std::vector<ShadowMapInstanceParams> shadow_instance_params;

    SwapchainPassParams swapchain_pass_params;
};

struct MeshCache;

void prepare_scene(const SceneGraph& scene, PreparedData& prepared, const MeshCache& mesh_cache);
} // namespace Reaper
