////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_LIGHTING_INCLUDED
#define SHARE_LIGHTING_INCLUDED

#include "types.hlsl"

struct PointLightProperties
{
    hlsl_float4x4 light_ws_to_cs; // FIXME xy could be in uv-space already
    hlsl_float3 position_vs;
    hlsl_float  intensity;
    hlsl_float3 color;
    hlsl_uint   shadow_map_index;
};

#endif
