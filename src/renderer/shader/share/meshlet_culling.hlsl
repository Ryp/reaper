////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_MESHLET_CULLING_INCLUDED
#define SHARE_MESHLET_CULLING_INCLUDED

#include "types.hlsl"

struct MeshletOffsets
{
    hlsl_uint index_offset; // Has the global index offset baked in
    hlsl_uint index_count;
    hlsl_uint vertex_offset; // Has the global vertex offset baked in
    hlsl_uint cull_instance_id;
};

static const hlsl_uint PrepareIndirectDispatchThreadCount = 16;
static const hlsl_uint MeshletCullThreadCount = 32;

static const hlsl_uint CountersCount = 3;

// Counter buffer layout
static const hlsl_uint MeshletCounterOffset = 0;
static const hlsl_uint TriangleCounterOffset = 1;
static const hlsl_uint DrawCommandCounterOffset = 2;

struct CullMeshletPushConstants
{
    hlsl_uint meshlet_offset;
    hlsl_uint meshlet_count;
    hlsl_uint first_index;
    hlsl_uint first_vertex;
    hlsl_uint cull_instance_offset;
    // No need for manual padding for push constants
};

struct CullPushConstants
{
    hlsl_float2 output_size_ts;
    // No need for manual padding for push constants
};

struct CullMeshInstanceParams
{
    hlsl_float4x4 ms_to_cs_matrix;
    hlsl_float3 vs_to_ms_matrix_translate; // FIXME Put this in somewhere else
    hlsl_uint instance_id;
};

#endif
