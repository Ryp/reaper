////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TiledLightingCommon.h"

#include "renderer/Camera.h"
#include "renderer/PrepareBuckets.h"
#include "renderer/vulkan/ComputeHelper.h"

#include "core/Assert.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Reaper
{
namespace
{
    static const bool TileLightingEnableConservativeRaster = true;

    // Math here is not exact, it's assuming a lot to keep it simple.
    // There's not a big perf loss normally.
    float compute_aabb_conservative_scale(glm::fvec3 aabb_half_extent, float distance_to_camera, glm::fvec2 fov_radians,
                                          glm::fvec2 tile_count_inv)
    {
        if (!TileLightingEnableConservativeRaster)
        {
            return 1.f;
        }

        static const float epsilon = 0.0001f;
        Assert(fov_radians.x > 0.f);
        Assert(fov_radians.y > 0.f);

        const float aabb_radius_underestimate =
            std::min(std::min(aabb_half_extent.x, aabb_half_extent.y), aabb_half_extent.z);

        const float aabb_projected_angular_diameter =
            2.f * std::atan(aabb_radius_underestimate / (std::max(epsilon, distance_to_camera)));

        const float aabb_fov_ratio_x = aabb_projected_angular_diameter / fov_radians.x;
        const float aabb_fov_ratio_y = aabb_projected_angular_diameter / fov_radians.y;

        const float tile_fov_ratio_x = tile_count_inv.x;
        const float tile_fov_ratio_y = tile_count_inv.y;

        const float padded_fov_ratio_x = aabb_fov_ratio_x + tile_fov_ratio_x;
        const float padded_fov_ratio_y = aabb_fov_ratio_y + tile_fov_ratio_y;

        const float scale_factor_x = padded_fov_ratio_x / std::max(aabb_fov_ratio_x, epsilon);
        const float scale_factor_y = padded_fov_ratio_y / std::max(aabb_fov_ratio_y, epsilon);

        const float conservative_scale = std::max(scale_factor_x, scale_factor_y);
        Assert(conservative_scale >= 1.f);

        return conservative_scale;
    }

} // namespace

void prepare_tile_lighting_frame(const SceneGraph& scene, const RendererPerspectiveCamera& main_camera,
                                 TiledLightingFrame& tiled_lighting_frame)
{
    tiled_lighting_frame.tile_count_x = div_round_up(main_camera.viewport.extent.x, TileSizeX);
    tiled_lighting_frame.tile_count_y = div_round_up(main_camera.viewport.extent.y, TileSizeY);

    for (u32 scene_light_index = 0; scene_light_index < scene.scene_lights.size(); scene_light_index++)
    {
        const SceneLight&  light = scene.scene_lights[scene_light_index];
        const glm::fmat4x3 light_ms_to_ws = get_scene_node_transform_slow(light.scene_node);
        const glm::fmat4x3 light_ws_to_ms = glm::inverse(glm::fmat4(light_ms_to_ws));

        const glm::vec3 light_position_ws = light_ms_to_ws * glm::vec4(0.f, 0.f, 0.f, 1.0f);
        const glm::vec3 light_position_vs =
            glm::fmat4x3(main_camera.ws_to_vs_matrix) * glm::fvec4(light_position_ws, 1.f);

        const glm::fvec3 light_aabb_half_extent = glm::fvec3(light.radius, light.radius, light.radius);
        const float      light_distance = glm::length(light_position_vs);

        glm::fvec2 fov_radians = {main_camera.perspective_projection.half_fov_horizontal_radian,
                                  main_camera.perspective_projection.half_fov_vertical_radian};
        glm::fvec2 tile_count_inv = {1.f / static_cast<float>(tiled_lighting_frame.tile_count_x),
                                     1.f / static_cast<float>(tiled_lighting_frame.tile_count_y)};

        const float conservative_scale =
            1.6f * compute_aabb_conservative_scale(light_aabb_half_extent, light_distance, fov_radians, tile_count_inv);

        const glm::fmat4 conservative_scale_matrix =
            glm::scale(glm::identity<glm::fmat4>(), glm::fvec3(1.f, 1.f, 1.f) * conservative_scale);

        // FIXME what happens to scale inheriting from a transform hierarchy?
        // FIXME Fill completely
        // FIXME should handle scales from light shape
        // FIXME cs_to_vs not per-instance
        LightVolumeInstance& light_volume = tiled_lighting_frame.light_volumes.emplace_back();
        light_volume.ms_to_cs = main_camera.ws_to_cs_matrix * glm::mat4(light_ms_to_ws);
        light_volume.cs_to_ms = glm::mat4(light_ws_to_ms) * main_camera.cs_to_ws_matrix;
        light_volume.cs_to_vs = main_camera.perspective_projection.cs_to_vs_matrix;
        light_volume.vs_to_ms =
            glm::mat4x3(glm::mat4(light_ws_to_ms) * glm::mat4(main_camera.vs_to_ws_matrix)); // FIXME
        light_volume.light_index = scene_light_index;
        light_volume.radius = light.radius * conservative_scale; // FIXME might be better baked in ms_to_cs

        ProxyVolumeInstance& proxy_volume = tiled_lighting_frame.proxy_volumes.emplace_back();
        const glm::fmat4     volume_scale = glm::scale(glm::fmat4(1.f), light_aabb_half_extent * conservative_scale);
        proxy_volume.ms_to_vs_with_scale = glm::mat4x3(glm::fmat4(main_camera.ws_to_vs_matrix)
                                                       * glm::fmat4(light_ms_to_ws) * volume_scale); // FIXME mul order
    }
}
} // namespace Reaper
