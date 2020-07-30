////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_CULLING_INCLUDED
#define SHARE_CULLING_INCLUDED

#include "types.hlsl"

static const hlsl_uint ComputeCullingGroupSize = 256;

struct CullPushConstants
{
    hlsl_uint triangleCount;
    hlsl_uint firstIndex;
};

struct CullInstanceParams
{
    hlsl_float4x4 ms_to_cs_matrix;
    hlsl_uint instance_id;
    hlsl_float _pad0;
    hlsl_float _pad1;
    hlsl_float _pad2;
};

#endif
