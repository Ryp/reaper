////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef HISTOGRAM_REDUCE_SHARE_INCLUDED
#define HISTOGRAM_REDUCE_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_float HistogramEVCount = 32.0;
static const hlsl_float HistogramEVOffset = 16.0;
static const hlsl_uint  HistogramRes = 128;

static const hlsl_uint HistogramThreadCountX = 8;
static const hlsl_uint HistogramThreadCountY = 8;

struct ReduceHDRPassParams
{
    hlsl_uint2  extent_ts;
    hlsl_float2 extent_ts_inv;
};

#endif
