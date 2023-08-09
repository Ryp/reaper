////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef TILED_LIGHTING_TILE_DEPTH_DOWNSAMPLE_SHARE_INCLUDED
#define TILED_LIGHTING_TILE_DEPTH_DOWNSAMPLE_SHARE_INCLUDED

#include "shared_types.hlsl"

// NOTE: Should always be a power of two
static const hlsl_uint MinWaveLaneCount = 8;

static const hlsl_uint TileDepthThreadCountX = 8;
static const hlsl_uint TileDepthThreadCountY = 8;

struct TileDepthConstants
{
    hlsl_uint2  extent_ts;
    hlsl_float2 extent_ts_inv;
};

#endif
