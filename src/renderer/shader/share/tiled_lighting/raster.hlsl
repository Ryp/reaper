////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_TILE_LIGHTING_RASTER_INCLUDED
#define SHARE_TILE_LIGHTING_RASTER_INCLUDED

#include "share/types.hlsl"

static const hlsl_uint ClassifyVolumeThreadCount = 64;

struct ClassifyVolumePushConstants
{
    hlsl_uint vertex_offset;
    hlsl_uint vertex_count;
    hlsl_uint instance_id_offset;
    hlsl_float near_clip_plane_depth_vs;
};

static const hlsl_uint InnerCounterOffsetBytes = 0;
static const hlsl_uint OuterCounterOffsetBytes = 4;

#endif
