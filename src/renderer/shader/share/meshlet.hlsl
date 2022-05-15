////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_MESHLET_INCLUDED
#define SHARE_MESHLET_INCLUDED

#include "types.hlsl"

static const hlsl_uint MeshletMaxTriangleCount = 64;

struct Meshlet
{
    hlsl_uint index_offset;
    hlsl_uint index_count;
    hlsl_uint vertex_offset;
    hlsl_uint vertex_count;
    hlsl_float3 center;
    hlsl_float radius;
};

#endif
