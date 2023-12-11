////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef DEBUG_GEOMETRY_PUBLIC_SHARE_INCLUDED
#define DEBUG_GEOMETRY_PUBLIC_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint DebugGeometryType_Icosphere = 0;
static const hlsl_uint DebugGeometryType_Box = 1;
static const hlsl_uint DebugGeometryTypeCount = 2;

struct DebugGeometryUserCommand
{
    hlsl_float3x4   ms_to_ws_matrix;
    hlsl_uint       geometry_type;
    hlsl_uint       color_rgba8_unorm;

    // FIXME Think of a better way to have some kind of polymorphism that isn't super ugly
    hlsl_uint       _pad0;
    hlsl_uint       _pad1;
    hlsl_float3     half_extent;
    hlsl_uint       _pad2;
};

static const hlsl_uint DebugGeometryUserCommandSizeBytes = 4 * (16 + 4 + 4);

#endif
