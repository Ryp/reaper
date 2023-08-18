////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHADOW_MAP_PASS_SHARE_INCLUDED
#define SHADOW_MAP_PASS_SHARE_INCLUDED

#include "shared_types.hlsl"

struct ShadowMapInstanceParams
{
    hlsl_float4x4 ms_to_cs_matrix;
};

#endif
