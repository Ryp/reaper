////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
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
    hlsl_uint firstVertex;
    hlsl_uint outputIndexOffset;
    hlsl_uint firstCullInstance;
    hlsl_uint _pad0;
    hlsl_uint _pad1;
    hlsl_uint _pad2;
};

struct CullPassParams
{
    hlsl_float2 output_size_ts;
    hlsl_float2 _pad;
};

struct CullMeshInstanceParams
{
    hlsl_float4x4 ms_to_cs_matrix;
    hlsl_uint instance_id;
    hlsl_float _pad0;
    hlsl_float _pad1;
    hlsl_float _pad2;
};

struct CullMeshletInstanceParams
{
    hlsl_uint index_offset;
    hlsl_uint index_count;
    hlsl_uint vertex_offset;
    hlsl_uint vertex_count;

    hlsl_uint3  aabb_min_ms;
    hlsl_uint   _pad0;
    hlsl_uint3  aabb_max_ms;
    hlsl_uint   _pad1;
};

#endif
