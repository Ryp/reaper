////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/shader/tiled_lighting/tiled_lighting.share.hlsl"

#include <vector>

namespace Reaper
{
struct SceneGraph;
struct RendererPerspectiveCamera;

struct TiledLightingFrame
{
    u32 tile_count_x;
    u32 tile_count_y;

    std::vector<LightVolumeInstance> light_volumes;
    std::vector<ProxyVolumeInstance> proxy_volumes;
};

void prepare_tile_lighting_frame(const SceneGraph& scene, const RendererPerspectiveCamera& main_camera,
                                 TiledLightingFrame& tiled_lighting_frame);
} // namespace Reaper
