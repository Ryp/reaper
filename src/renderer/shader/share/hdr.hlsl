////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_HDR_INCLUDED
#define SHARE_HDR_INCLUDED

#include "types.hlsl"

static const hlsl_float HistogramEVCount = 32.0;
static const hlsl_float HistogramEVOffset = 16.0;
static const hlsl_uint  HistogramRes = 512;

static const hlsl_uint HistogramThreadCountX = 8;
static const hlsl_uint HistogramThreadCountY = 8;

struct ReduceHDRPassParams
{
    hlsl_uint2  extent_ts;
    hlsl_float2 extent_ts_inv;
};

#endif
