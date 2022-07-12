////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef RASTERIZE_LIGHT_VOLUME_INCLUDED
#define RASTERIZE_LIGHT_VOLUME_INCLUDED

#include "share/tiled_lighting.hlsl"

uint get_tile_index_flat(uint2 tile_index, uint tile_count_x)
{
    return tile_index.y * tile_count_x + tile_index.x;
}

uint get_tile_offset(uint tile_index_flat)
{
    return tile_index_flat * ElementsPerTile;
}

float3 heatmap_color(float value)
{
    const uint g_color_count = 5;

    const float3 g_colors[g_color_count] = {
        float3(0.0, 0.0, 1.0),
        float3(0.0, 1.0, 1.0),
        float3(0.0, 1.0, 0.0),
        float3(1.0, 1.0, 0.0),
        float3(1.0, 0.0, 0.0)
    };

    const float epsilon = 0.0001;
    value = min(max(value, epsilon), 1.0 - epsilon);

    const float index = value * (float)(g_color_count - 1);

    const uint color_index = trunc(index);
    const float t = frac(index);

    const float3 color_a = g_colors[color_index];
    const float3 color_b = g_colors[color_index + 1];

    return lerp(color_a, color_b, t);
}

struct PassParams
{
    LightVolumeInstance instance_array[LightVolumeMax];
};

// https://github.com/KhronosGroup/glslang/issues/1629
#if defined(_DXC)
VK_PUSH_CONSTANT() TileLightRasterPushConstants consts;
#else
VK_PUSH_CONSTANT() ConstantBuffer<TileLightRasterPushConstants> consts;
#endif
VK_BINDING(0, 0) ConstantBuffer<PassParams> pass_params;
VK_BINDING(1, 0) Texture2D<float> TileDepth;
VK_BINDING(2, 0) globallycoherent RWByteAddressBuffer VisibleLightIndices;

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 position_cs : TEXCOORD0; // Without SV_POSITION semantics
    nointerpolation uint instance_id : TEXCOORD1;
};

#endif
