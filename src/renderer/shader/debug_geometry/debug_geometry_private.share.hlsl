////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef DEBUG_GEOMETRY_PRIVATE_SHARE_INCLUDED
#define DEBUG_GEOMETRY_PRIVATE_SHARE_INCLUDED

#include "shared_types.hlsl"

#include "debug_geometry_public.share.hlsl"

struct DebugGeometryMeshAlloc
{
    hlsl_uint index_offset;
    hlsl_uint index_count;
    hlsl_uint vertex_offset;
    hlsl_uint _pad;
};

struct DebugGeometryBuildCmdsPassConstants
{
    hlsl_float4x4 main_camera_ws_to_cs;
    DebugGeometryMeshAlloc debug_geometry_allocs[DebugGeometryTypeCount];
};

static const hlsl_uint DebugGeometryBuildCmdsThreadCount = 128;

struct DebugGeometryInstance
{
    hlsl_float4x4   ms_to_cs;
    hlsl_float3     color;
    hlsl_float      _pad0;
    hlsl_float3     half_extent;
    hlsl_float      _pad1;
};

static const hlsl_uint DebugGeometryInstanceSizeBytes = 4 * (16 + 4 + 4);

#endif
