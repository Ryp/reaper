////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2024 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REDUCE_EXPOSURE_SHARE_INCLUDED
#define REDUCE_EXPOSURE_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint MinWaveLaneCount = 8; // FIXME

static const hlsl_uint ExposureThreadCountX = 8;
static const hlsl_uint ExposureThreadCountY = 8;

struct ReduceExposurePassParams
{
    hlsl_uint2  extent_ts;
    hlsl_float2 extent_ts_inv;
};

struct ReduceExposureTailPassParams
{
    hlsl_uint2  extent_ts;
    hlsl_float2 extent_ts_inv;
    hlsl_uint2  tail_extent_ts;
    hlsl_uint   last_thread_group_index;
};

#endif
