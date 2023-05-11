////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_DEBUG_GEOMETRY_PUBLIC_INCLUDED
#define SHARE_DEBUG_GEOMETRY_PUBLIC_INCLUDED

#include "types.hlsl"

static const hlsl_uint DebugGeometryTypeCount = 1;
static const hlsl_uint DebugGeometryType_Icosphere = 0;

struct DebugGeometryUserCommand
{
    hlsl_float3x4   ms_to_ws_matrix;
    hlsl_uint       geometry_type;
    hlsl_uint       color_rgba8_unorm;
    hlsl_float      radius;
    hlsl_uint       _pad;
};

#endif
