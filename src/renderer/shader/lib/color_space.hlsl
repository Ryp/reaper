////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_COLOR_SPACE_INCLUDED
#define LIB_COLOR_SPACE_INCLUDED

// FIXME This is not exactly what vulkan does.
// See https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.html#TRANSFER_SRGB
float3 linear_to_srgb(float3 linear_color)
{
    return pow(linear_color, 1.0 / 2.2);
}

float3 srgb_to_linear(float3 srgb_color)
{
    return pow(srgb_color, 2.2);
}

#endif
