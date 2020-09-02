////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_COMMON_INCLUDED
#define LIB_COMMON_INCLUDED

float min3(float3 v) { return min(min(v.x, v.y), v.z); }
float max3(float3 v) { return max(max(v.x, v.y), v.z); }

float2 ndc_to_ts(float2 position_ndc, float2 extent_ts)
{
    return (position_ndc * 0.5 + 0.5) * extent_ts;
}

#endif
