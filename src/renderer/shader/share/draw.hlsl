////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_DRAW_INCLUDED
#define SHARE_DRAW_INCLUDED

#include "types.hlsl"

struct DrawPassParams
{
    hlsl_float4x4 viewProj;
    hlsl_float3   light_position_vs;
    hlsl_float    timeMs;
    hlsl_float3   light_direction_vs;
    hlsl_float    _pad;
};

struct DrawInstanceParams
{
    hlsl_float3x4 model;
    hlsl_float3x3 normal_ms_to_vs_matrix;
};

#endif
