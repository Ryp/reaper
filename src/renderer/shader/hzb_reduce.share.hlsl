////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef HZB_REDUCE_SHARE_INCLUDED
#define HZB_REDUCE_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint MinWaveLaneCount = 8; // FIXME

static const hlsl_uint HZBMaxMipCount = 10;

static const hlsl_uint HZBReduceThreadCountX = 8;
static const hlsl_uint HZBReduceThreadCountY = 8;

struct HZBReducePushConstants
{
    hlsl_float2 depth_extent_ts_inv;
};

#endif
