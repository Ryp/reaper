////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_SHADOW_MAP_PASS_INCLUDED
#define SHARE_SHADOW_MAP_PASS_INCLUDED

#include "types.hlsl"

struct ShadowMapPassParams
{
    hlsl_float4x4 dummy;
};

struct ShadowMapInstanceParams
{
    hlsl_float4x4 ms_to_cs_matrix;
};

#endif
