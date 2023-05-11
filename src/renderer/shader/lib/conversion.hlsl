////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_CONVERSION_INCLUDED
#define LIB_CONVERSION_INCLUDED

int float_to_snorm_clamp(float v, uint bits)
{
    float v_clamp = max(min(v, 1.0), -1.0);
    uint max_value = (1u << (bits - 1)) - 1;

    return (int)trunc(v_clamp * max_value + (v_clamp >= 0.f ? 0.5f : -0.5f));
}

float4 rgba8_unorm_to_rgba32_float(uint rgba8_unorm)
{
    const uint4 unorm_split = uint4(
        rgba8_unorm,
        rgba8_unorm >> 8,
        rgba8_unorm >> 16,
        rgba8_unorm >> 24);

    return float4(unorm_split & 0xFF) / 255.f;
}

#endif
