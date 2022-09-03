////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_LIGHTING_INCLUDED
#define SHARE_LIGHTING_INCLUDED

#include "types.hlsl"

static const hlsl_uint InvalidShadowMapIndex = 0xFFFFFFFF;

struct PointLightProperties
{
    hlsl_float4x4 light_ws_to_cs; // FIXME xy could be in uv-space already
    hlsl_float3 position_vs;
    hlsl_float  intensity;
    hlsl_float3 color;
    hlsl_float  radius_sq;
    hlsl_uint   shadow_map_index;
    hlsl_uint   _pad0;
    hlsl_uint   _pad1;
    hlsl_uint   _pad2;
};

#endif
