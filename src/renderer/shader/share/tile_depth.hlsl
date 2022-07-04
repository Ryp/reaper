////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef TILE_DEPTH_INCLUDED
#define TILE_DEPTH_INCLUDED

#include "types.hlsl"

static const hlsl_uint TileDepthThreadCountX = 8;
static const hlsl_uint TileDepthThreadCountY = 8;

struct TileDepthConstants
{
    hlsl_uint2  extent_ts;
    hlsl_float2 extent_ts_inv;
};

#endif
