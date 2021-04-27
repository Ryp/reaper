////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_HDR_INCLUDED
#define SHARE_HDR_INCLUDED

#include "types.hlsl"

static const hlsl_float HISTOGRAM_EV_COUNT = 32.0;
static const hlsl_float HISTOGRAM_EV_OFFSET = 16.0;
static const hlsl_uint  HISTOGRAM_RES = 1024;

static const hlsl_uint ReduceHDRGroupSizeX = 8;
static const hlsl_uint ReduceHDRGroupSizeY = 8;

struct ReduceHDRPassParams
{
    hlsl_uint2 input_size_ts;
    hlsl_uint2 _pad;
};

#endif
