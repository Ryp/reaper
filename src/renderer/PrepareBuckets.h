////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include <glm/glm.hpp>

#include <vector>

#include "renderer/shader/share/compaction.hlsl"
#include "renderer/shader/share/culling.hlsl"
#include "renderer/shader/share/draw.hlsl"
#include "renderer/shader/share/shadow_map_pass.hlsl"

namespace Reaper
{
struct GirugaMesh;

struct Light
{
    glm::mat4 projection_matrix;
    glm::vec3 color;
    float     intensity;
    u32       scene_node;
};

struct Node
{
    glm::mat4x3 transform_matrix;
    GirugaMesh* mesh;
};

struct SceneCamera
{
    glm::mat4 projection_matrix;
    u32       scene_node;
};

struct SceneGraph
{
    std::vector<Node> nodes;

    std::vector<Light> lights;
    SceneCamera        camera;
};

void build_scene_graph(SceneGraph& scene);
void update_scene_graph(SceneGraph& scene, float time_ms, float aspect_ratio, const glm::mat4x3& view_matrix);

struct PreparedData
{
    std::vector<CullInstanceParams>      cull_instance_params;
    DrawPassParams                       draw_pass_params;
    std::vector<DrawInstanceParams>      draw_instance_params;
    ShadowMapPassParams                  shadow_pass_params;
    std::vector<ShadowMapInstanceParams> shadow_instance_params;
};

void prepare_scene(SceneGraph& scene, PreparedData& prepared);
} // namespace Reaper