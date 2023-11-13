////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef TILED_LIGHTING_SHARE_INCLUDED
#define TILED_LIGHTING_SHARE_INCLUDED

#include "shared_types.hlsl"

// Has to be a multiple of 8
static const hlsl_uint TileSizeX = 16;
static const hlsl_uint TileSizeY = 16;

// Has to be a multiple of TileSizeX
static const hlsl_uint TiledLightingThreadCountX = 16;
static const hlsl_uint TiledLightingThreadCountY = 16;

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
    hlsl_float4x4   cs_to_vs; // FIXME
    hlsl_float3x4   vs_to_ms;
    hlsl_uint       light_index;
    hlsl_float      radius;
    hlsl_uint       _pad0;
    hlsl_uint       _pad1;
};

struct ProxyVolumeInstance
{
    hlsl_float3x4 ms_to_vs_with_scale;
};

struct TiledLightingConstants
{
    hlsl_float4x4 cs_to_vs;
    hlsl_float3x4 vs_to_ws;
};

// No need for padding here
struct TiledLightingPushConstants
{
    hlsl_uint2  extent_ts;
    hlsl_float2 extent_ts_inv;
    hlsl_uint   tile_count_x;
};

struct TileDebug
{
    hlsl_uint   light_count;
    hlsl_uint   _pad0;
    hlsl_uint   _pad1;
    hlsl_uint   _pad2;
};

// No need for padding here
struct TiledLightingDebugPushConstants
{
    hlsl_uint2  extent_ts;
    hlsl_uint   tile_count_x;
};

#endif
