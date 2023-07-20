////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_FORMAT_UNORM_INCLUDED
#define LIB_FORMAT_UNORM_INCLUDED

////////////////////////////////////////////////////////////////////////////////
// Generic float<->unorm conversion routines that work with variable bit depths.
//
// NOTE:
// - Float input is ASSUMED to be in [0.0, 1.0].
//   Uint input can have extra bits outside of the encoded range for
//   multiple-channel versions.
//
// - Do NOT use values larger than 24 for the bit depth and make it known
//   at compile-time otherwise you'll generate slow code.
//
// - The return type is restricted to 32 bits at the moment.
//
// float -> unorm
uint r32_float_to_unorm_generic(float r32_float, uint bits_per_channel)
{
    const uint max_value = (1u << bits_per_channel) - 1; // Ex: 255 or 0xFF for 8 bits

    return uint(r32_float * float(max_value));
}

uint rg32_float_to_unorm_generic(float2 rg32_float, uint bits_per_channel)
{
    const uint max_value = (1u << bits_per_channel) - 1; // Ex: 255 or 0xFF for 8 bits

    const uint2 rg_split = uint2(rg32_float * float(max_value));

    const uint rg_unorm = rg_split.r
                        | rg_split.g << bits_per_channel;

    return rg_unorm;
}

uint rgb32_float_to_unorm_generic(float3 rgb32_float, uint bits_per_channel)
{
    const uint max_value = (1u << bits_per_channel) - 1; // Ex: 255 or 0xFF for 8 bits

    const uint3 rgb_split = uint3(rgb32_float * float(max_value));

    const uint rgb_unorm = rgb_split.r
                         | rgb_split.g << bits_per_channel
                         | rgb_split.b << (bits_per_channel * 2);

    return rgb_unorm;
}

uint rgba32_float_to_unorm_generic(float4 rgba32_float, uint bits_per_channel)
{
    const uint max_value = (1u << bits_per_channel) - 1; // Ex: 255 or 0xFF for 8 bits

    const uint4 rgba_split = uint4(rgba32_float * float(max_value));

    const uint rgba_unorm = rgba_split.r
                          | rgba_split.g << bits_per_channel
                          | rgba_split.b << (bits_per_channel * 2)
                          | rgba_split.a << (bits_per_channel * 3);

    return rgba_unorm;
}

// unorm -> float
float r_unorm_generic_to_r32_float(uint r_unorm, uint bits_per_channel)
{
    const uint max_value = (1u << bits_per_channel) - 1; // Ex: 255 or 0xFF for 8 bits

    return float(r_unorm) / float(max_value);
}

float2 rg_unorm_generic_to_rg32_float(uint rg_unorm, uint bits_per_channel)
{
    const uint max_value = (1u << bits_per_channel) - 1; // Ex: 255 or 0xFF for 8 bits

    const uint2 unorm_split = uint2(
        rg_unorm,
        rg_unorm >> bits_per_channel);

    return float2(unorm_split & max_value) / float(max_value);
}

float3 rgb_unorm_generic_to_rgb32_float(uint rgb_unorm, uint bits_per_channel)
{
    const uint max_value = (1u << bits_per_channel) - 1; // Ex: 255 or 0xFF for 8 bits

    const uint3 unorm_split = uint3(
        rgb_unorm,
        rgb_unorm >> bits_per_channel,
        rgb_unorm >> (bits_per_channel * 2));

    return float3(unorm_split & max_value) / float(max_value);
}

float4 rgba_unorm_generic_to_rgba32_float(uint rgba_unorm, uint bits_per_channel)
{
    const uint max_value = (1u << bits_per_channel) - 1; // Ex: 255 or 0xFF for 8 bits

    const uint4 unorm_split = uint4(
        rgba_unorm,
        rgba_unorm >> bits_per_channel,
        rgba_unorm >> (bits_per_channel * 2),
        rgba_unorm >> (bits_per_channel * 3));

    return float4(unorm_split & max_value) / float(max_value);
}

////////////////////////////////////////////////////////////////////////////////
// Specialized float<->unorm conversion routines
//
// NOTE: Float input is ASSUMED to be in [0.0, 1.0].
//
// unorm1x8
uint r32_float_to_r8_unorm(float r32_float)
{
    return r32_float_to_unorm_generic(r32_float, 8);
}

float r8_unorm_to_r32_float(uint r8_unorm)
{
    return r_unorm_generic_to_r32_float(r8_unorm, 8);
}

// unorm3x8
uint rgb32_float_to_rgb8_unorm(float3 rgb32_float)
{
    return rgb32_float_to_unorm_generic(rgb32_float, 8);
}

float3 rgb8_unorm_to_rgb32_float(uint rgb8_unorm)
{
    return rgb_unorm_generic_to_rgb32_float(rgb8_unorm, 8);
}

// unorm4x8
uint rgba32_float_to_rgba8_unorm(float4 rgba32_float)
{
    return rgba32_float_to_unorm_generic(rgba32_float, 8);
}

float4 rgba8_unorm_to_rgba32_float(uint rgba8_unorm)
{
    return rgba_unorm_generic_to_rgba32_float(rgba8_unorm, 8);
}

#endif
