////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef MESHLET_SHARE_INCLUDED
#define MESHLET_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint MeshletMaxTriangleCount = 64;

struct Meshlet
{
    hlsl_uint   index_offset;
    hlsl_uint   index_count;
    hlsl_uint   vertex_offset;
    hlsl_uint   vertex_count;
    hlsl_float3 center_ms;
    hlsl_float  radius;
    hlsl_float3 cone_axis_ms;
    hlsl_float  cone_cutoff; /* = cos(angle/2) */
    hlsl_float3 cone_apex_ms;
    hlsl_float  _pad;
};

struct VisibleMeshlet
{
    hlsl_uint mesh_instance_id;
    hlsl_uint visible_triangle_offset;
    hlsl_uint vertex_offset;
    hlsl_float  _pad;
};

#endif
