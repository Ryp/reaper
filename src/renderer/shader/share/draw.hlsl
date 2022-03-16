////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_DRAW_INCLUDED
#define SHARE_DRAW_INCLUDED

#include "types.hlsl"

struct PointLightProperties
{
    hlsl_float4x4 light_ws_to_cs; // FIXME xy could be in uv-space already
    hlsl_float3 position_vs;
    hlsl_float  intensity;
    hlsl_float3 color;
    hlsl_float  shadow_map_index;
};

static const hlsl_uint PointLightCount = 3;

struct DrawPassParams
{
    PointLightProperties point_light[PointLightCount];
    hlsl_float3x4 ws_to_vs_matrix;
    hlsl_float4x4 vs_to_cs_matrix;
    hlsl_float4x4 ws_to_cs_matrix;
};

struct DrawInstanceParams
{
    hlsl_float3x4 ms_to_ws_matrix;
    hlsl_float3x3 normal_ms_to_vs_matrix;
    hlsl_uint3    _pad;
    hlsl_uint     texture_index;
};

#endif
