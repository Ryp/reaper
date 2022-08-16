////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_TILED_LIGHTING_INCLUDED
#define SHARE_TILED_LIGHTING_INCLUDED

#include "types.hlsl"

// Has to be a multiple of 8!
static const hlsl_uint TileSizeX = 16;
static const hlsl_uint TileSizeY = 16;

static const hlsl_uint LightVolumeMax = 128;

// Remove one to leave space for the count
static const hlsl_uint ElementsPerTile = 16;
static const hlsl_uint LightsPerTileMax = ElementsPerTile - 1;

struct TileLightRasterPushConstants
{
    hlsl_uint instance_id_offset;
    hlsl_uint tile_count_x;
};

struct LightVolumeInstance
{
    hlsl_float4x4   ms_to_cs;
    hlsl_float4x4   cs_to_ms;
    hlsl_float4x4   cs_to_vs;
    hlsl_float3x4   vs_to_ms;
    hlsl_uint       light_index;
    hlsl_uint       _pad0;
    hlsl_uint       _pad1;
    hlsl_uint       _pad2;
};

#endif