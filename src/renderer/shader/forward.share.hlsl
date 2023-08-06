////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef FORWARD_SHARE_INCLUDED
#define FORWARD_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint DiffuseMapMaxCount = 8;
static const hlsl_uint ShadowMapMaxCount  = 8;

struct ForwardPassParams
{
    hlsl_float3x4 ws_to_vs_matrix;
    hlsl_float4x4 vs_to_cs_matrix;
    hlsl_float4x4 ws_to_cs_matrix;
    hlsl_uint3    _pad;
    hlsl_uint     point_light_count;
};

struct ForwardInstanceParams
{
    hlsl_float4x4 ms_to_cs_matrix; // FIXME
    hlsl_float3x4 ms_to_ws_matrix;
    hlsl_float3x3 normal_ms_to_vs_matrix;
    hlsl_uint3    _pad;
    hlsl_uint     texture_index;
};

#endif
