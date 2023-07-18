////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_CONVERSION_INCLUDED
#define LIB_CONVERSION_INCLUDED

uint float_to_snorm_generic(float v, uint bits)
{
    uint max_value = (1u << (bits - 1)) - 1;
    int snorm_value = (int)trunc(v * max_value + (v >= 0.f ? 0.5f : -0.5f));

    return asuint(snorm_value);
}

// Clamps to snorm range
uint float_to_snorm_generic_safe(float v, uint bits)
{
    float v_clamp = max(min(v, 1.0), -1.0);

    return float_to_snorm_generic(v_clamp, bits);
}

float r8_unorm_to_r32_float(uint r8_unorm)
{
    return float(r8_unorm) / 255.f;
}

float3 rgb8_unorm_to_rgb32_float(uint rgb8_unorm)
{
    const uint3 unorm_split = uint3(
        rgb8_unorm,
        rgb8_unorm >> 8,
        rgb8_unorm >> 16);

    return float3(unorm_split & 0xFF) / 255.f;
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

uint r32_float_to_r8_unorm(float r32_float)
{
    return r32_float * 255.f;
}

uint rgb32_float_to_rgb8_unorm(float3 rgb32_float)
{
    const uint3 rgb_uint = uint3(rgb32_float * 255.f);

    const uint rgb8_unorm =
        rgb_uint.r |
        rgb_uint.g << 8 |
        rgb_uint.b << 16;

    return rgb8_unorm;
}

uint rgba32_float_to_rgba8_unorm(float4 rgba32_float)
{
    const uint4 rgba_uint = uint4(rgba32_float * 255.f);

    const uint rgba8_unorm =
        rgba_uint.r |
        rgba_uint.g << 8 |
        rgba_uint.b << 16 |
        rgba_uint.a << 24;

    return rgba8_unorm;
}

// Clamps to unorm range
uint rgba32_float_to_rgba8_unorm_safe(float4 rgba32_float)
{
    return rgba32_float_to_rgba8_unorm(saturate(rgba32_float));
}

uint rgb32_float_to_rgb8_snorm(float3 rgb32_float)
{
    return rgb32_float_to_rgb8_unorm(rgb32_float * 0.5 + 0.5);

    //return float_to_snorm_generic(rgb32_float.x, 8)
    //     | float_to_snorm_generic(rgb32_float.y, 8) << 8
    //     | float_to_snorm_generic(rgb32_float.z, 8) << 16;
}

float3 rgb8_snorm_to_rgb32_float(uint rgb8_snorm)
{
    return rgb8_unorm_to_rgb32_float(rgb8_snorm) * 2.0 - 1.0;

    // const uint3 rgb8_split = uint3(
    //     rgb8_snorm,
    //     rgb8_snorm >> 8,
    //     rgb8_snorm >> 16);

    // return (float3(asint(rgb8_split & 0xFF)) / 255.f) * 2.0 - 1.0;
}

#endif
