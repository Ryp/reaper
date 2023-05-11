////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef DEBUG_GEOMETRY_DRAW_INCLUDED
#define DEBUG_GEOMETRY_DRAW_INCLUDED

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    nointerpolation float3 color : TEXCOORD0;
};

#endif
