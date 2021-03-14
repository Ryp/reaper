////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_COLOR_SPACE_INCLUDED
#define LIB_COLOR_SPACE_INCLUDED

float3 rec709_to_rec2020(float3 color_rec709)
{
    static const float3x3 rec709_to_rec2020_matrix =
    {
        0.627402, 0.329292, 0.043306,
        0.069095, 0.919544, 0.011360,
        0.016394, 0.088028, 0.895578
    };

    return mul(rec709_to_rec2020_matrix, color_rec709);
}

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

float3 linear_to_rec709(float3 linear_color)
{
    return pow(linear_color, 1.0 / 2.4);
}

float3 rec709_to_linear(float3 rec709_color)
{
    return pow(rec709_color, 2.4);
}

float3 linear_to_pq(float3 linear_color)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;

    float3 Lp = pow(linear_color, m1);
    return pow((c1 + c2 * Lp) / (1 + c3 * Lp), m2);
}

float3 pq_to_linear(float3 pq_color)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;

    float3 Np = pow(pq_color, 1.0 / m2);
    return pow(max(Np - c1, 0.0) / (c2 - c3 * Np), 1 / m1);
}

#endif
