////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_FORMAT_SNORM_INCLUDED
#define LIB_FORMAT_SNORM_INCLUDED

#include "unorm.hlsl"

////////////////////////////////////////////////////////////////////////////////
// Generic float<->snorm conversion routines that work with variable bit depths.
//
// - Float input is ASSUMED to be in [-1.0, 1.0].
//   Uint input can have extra bits outside of the encoded range for
//   multiple-channel versions.
//
// - Do NOT use values larger than 24 for the bit depth and make it known
//   at compile-time otherwise you'll generate slow code.
//
// - The return type is restricted to 32 bits at the moment.
//
// float -> snorm
uint r32_float_to_snorm_generic(float r32_float, uint bits_per_channel)
{
    return r32_float_to_unorm_generic(r32_float * 0.5 + 0.5, bits_per_channel);
}

uint rg32_float_to_snorm_generic(float2 rg32_float, uint bits_per_channel)
{
    return rg32_float_to_unorm_generic(rg32_float * 0.5 + 0.5, bits_per_channel);
}

uint rgb32_float_to_snorm_generic(float3 rgb32_float, uint bits_per_channel)
{
    return rgb32_float_to_unorm_generic(rgb32_float * 0.5 + 0.5, bits_per_channel);
}

uint rgba32_float_to_snorm_generic(float4 rgba32_float, uint bits_per_channel)
{
    return rgba32_float_to_unorm_generic(rgba32_float * 0.5 + 0.5, bits_per_channel);
}

// snorm -> float
float r_snorm_generic_to_r32_float(uint r_snorm, uint bits_per_channel)
{
    return r_unorm_generic_to_r32_float(r_snorm, bits_per_channel) * 2.0 - 1.0;
}

float2 rg_snorm_generic_to_rg32_float(uint rg_snorm, uint bits_per_channel)
{
    return rg_unorm_generic_to_rg32_float(rg_snorm, bits_per_channel) * 2.0 - 1.0;
}

float3 rgb_snorm_generic_to_rgb32_float(uint rgb_snorm, uint bits_per_channel)
{
    return rgb_unorm_generic_to_rgb32_float(rgb_snorm, bits_per_channel) * 2.0 - 1.0;
}

float4 rgba_snorm_generic_to_rgba32_float(uint rgba_snorm, uint bits_per_channel)
{
    return rgba_unorm_generic_to_rgba32_float(rgba_snorm, bits_per_channel) * 2.0 - 1.0;
}

////////////////////////////////////////////////////////////////////////////////
// Specialized float<->snorm conversion routines
//
// NOTE: Float input is ASSUMED to be in [-1.0, 1.0].
//
// snorm1x8
uint r32_float_to_r8_snorm(float r32_float)
{
    return r32_float_to_snorm_generic(r32_float, 8);
}

float r8_snorm_to_r32_float(uint r8_snorm)
{
    return r_snorm_generic_to_r32_float(r8_snorm, 8);
}

// snorm3x8
uint rgb32_float_to_rgb8_snorm(float3 rgb32_float)
{
    return rgb32_float_to_snorm_generic(rgb32_float, 8);
}

float3 rgb8_snorm_to_rgb32_float(uint rgb8_snorm)
{
    return rgb_snorm_generic_to_rgb32_float(rgb8_snorm, 8);
}

// snorm4x8
uint rgba32_float_to_rgba8_snorm(float4 rgba32_float)
{
    return rgba32_float_to_snorm_generic(rgba32_float, 8);
}

float4 rgba8_snorm_to_rgba32_float(uint rgba8_snorm)
{
    return rgba_snorm_generic_to_rgba32_float(rgba8_snorm, 8);
}

#endif
