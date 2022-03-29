////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_EOTF_INCLUDED
#define LIB_EOTF_INCLUDED

float3 srgb_eotf(float3 color)
{
    // Looks like the gamma 2.2 function
    return (color < 0.0031308) ? 12.92 * color : 1.055 * pow(color, 1.0 / 2.4) - 0.055;
}

float3 srgb_eotf_inverse(float3 x)
{
    // Looks like the gamma 2.2 inverse function
    return (x < 0.04045) ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

// Replaces pow() with cheaper sqrt()
// Error < 0.4%
float3 srgb_eotf_fast(float3 color)
{
    return (color < 0.0031308) ? 12.92 * color : 1.13005 * sqrt(color - 0.00228) - 0.13448 * color + 0.005719;
}

float3 srgb_eotf_inverse_fast(float3 x)
{
    return (x < 0.04045) ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
}

// -----------------------------------------------------------------------------

float3 rec709_eotf(float3 color)
{
    return (color < 0.0181) ? 4.5 * color : 1.0993 * pow(color, 0.45) - 0.0993;
}

float3 rec709_eotf_inverse(float3 x)
{
    return (x < 0.08145) ? x / 4.5 : pow((x + 0.0993) / 1.0993, 1.0 / 0.45);
}

// -----------------------------------------------------------------------------

static const float PQ_M1 = 2610.0 / 4096.0 / 4.0;
static const float PQ_M2 = 2523.0 / 4096.0 * 128.0;
static const float PQ_C1 = 3424.0 / 4096.0;
static const float PQ_C2 = 2413.0 / 4096.0 * 32.0;
static const float PQ_C3 = 2392.0 / 4096.0 * 32.0;

float3 pq_eotf(float3 color)
{
    float3 Lp = pow(color, PQ_M1);
    return pow((PQ_C1 + PQ_C2 * Lp) / (1.0 + PQ_C3 * Lp), PQ_M2);
}

float3 pq_eotf_inverse(float3 pq_eotf_color)
{
    float3 Np = pow(pq_eotf_color, 1.0 / PQ_M2);
    return pow(max(Np - PQ_C1, 0.0) / (PQ_C2 - PQ_C3 * Np), 1.0 / PQ_M1);
}

// -----------------------------------------------------------------------------

float3 gamma22(float3 color)
{
    return pow(color, 1.0 / 2.2);
}

float3 gamma22_inverse(float3 x)
{
    return pow(x, 2.2);
}

// -----------------------------------------------------------------------------

float3 gamma24(float3 color)
{
    return pow(color, 1.0 / 2.4);
}

float3 gamma24_inverse(float3 x)
{
    return pow(x, 2.4);
}

#endif
