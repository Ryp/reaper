////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_DRAW_INCLUDED
#define SHARE_DRAW_INCLUDED

#include "types.hlsl"

struct PointLightProperties
{
    hlsl_float3 position_vs;
    hlsl_float  intensity;
    hlsl_float3 color;
    hlsl_float  _pad;
};

struct DrawPassParams
{
    PointLightProperties point_light;
    hlsl_float3x4 view;
    hlsl_float4x4 proj;
    hlsl_float4x4 view_proj;
    hlsl_float3   light_position_vs;
};

struct DrawInstanceParams
{
    hlsl_float3x4 model;
    hlsl_float3x3 normal_ms_to_vs_matrix;
};

#endif
