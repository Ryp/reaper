////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef MESH_INSTANCE_SHARE_INCLUDED
#define MESH_INSTANCE_SHARE_INCLUDED

#include "shared_types.hlsl"

// FIXME move out?
static const hlsl_uint MaterialTextureMaxCount = 16;
static const hlsl_uint ShadowMapMaxCount  = 8;

struct MeshInstance
{
    hlsl_float4x4 ms_to_cs_matrix; // FIXME
    hlsl_float3x4 ms_to_ws_matrix;
    hlsl_float3x3 normal_ms_to_vs_matrix;
    hlsl_uint3    _pad;
    hlsl_uint     material_index;
};

#endif
