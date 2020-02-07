////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_DRAW_INCLUDED
#define SHARE_DRAW_INCLUDED

#include "types.hlsl"

struct DrawPassParams
{
    sl_float4x4 viewProj;
    sl_float    timeMs;
};

struct DrawInstanceParams
{
    sl_float4x4 model;
    sl_float3x3 modelViewInvT;
};

#endif
