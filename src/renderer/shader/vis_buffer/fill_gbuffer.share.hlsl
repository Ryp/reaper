////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef VIS_BUFFER_FILL_GBUFFER_SHARE_INCLUDED
#define VIS_BUFFER_FILL_GBUFFER_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint GBufferFillThreadCountX = 16;
static const hlsl_uint GBufferFillThreadCountY = 16;

struct FillGBufferPushConstants
{
    hlsl_uint2 extent_ts;
    hlsl_float2 extent_ts_inv;
};

#endif
