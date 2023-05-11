////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_DEBUG_GEOMETRY_PRIVATE_INCLUDED
#define SHARE_DEBUG_GEOMETRY_PRIVATE_INCLUDED

#include "types.hlsl"

#include "share/debug_geometry_public.hlsl"

static const hlsl_uint DebugGeometryUserCommandSizeBytes = 4 * (16 + 4);

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

static const hlsl_uint DebugGeometryCommandSizeBytes = 4 * 4 * 2;

struct DebugGeometryCommand
{
    hlsl_float3 position_vs;
    hlsl_float  intensity;
    hlsl_float3 color;
    hlsl_float  radius_sq;
};

static const hlsl_uint DebugGeometryInstanceSizeBytes = 4 * (16 + 4);

struct DebugGeometryInstance
{
    hlsl_float4x4   ms_to_cs;
    hlsl_float3     color;
    hlsl_float      radius;
};

#endif
