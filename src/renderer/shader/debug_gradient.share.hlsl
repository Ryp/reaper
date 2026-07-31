////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2026 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef DEBUG_GRADIENT_SHARE_INCLUDED
#define DEBUG_GRADIENT_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint DebugGradientThreadCountX = 8;
static const hlsl_uint DebugGradientThreadCountY = 8;

struct DebugGradientPushConstants
{
    hlsl_uint2  extent_ts;
    hlsl_float2 extent_ts_inv;
};

#endif
