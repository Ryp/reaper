////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_TRANSFER_FUNCTIONS_INCLUDED
#define LIB_TRANSFER_FUNCTIONS_INCLUDED

// -----------------------------------------------------------------------------
// sRGB

float3 linear_to_srgb(float3 color_linear)
{
    return select((color_linear < 0.0031308), 12.92 * color_linear, 1.055 * pow(color_linear, 1.0 / 2.4) - 0.055);
}

float3 srgb_to_linear(float3 color_srgb)
{
    return select((color_srgb < 0.04045), color_srgb / 12.92, pow((color_srgb + 0.055) / 1.055, 2.4));
}

// Replaces pow() with cheaper sqrt()
// Error < 0.4%
float3 linear_to_srgb_fast(float3 color_linear)
{
    return select((color_linear < 0.0031308), 12.92 * color_linear, 1.13005 * sqrt(color_linear - 0.00228) - 0.13448 * color_linear + 0.005719);
}

float3 srgb_to_linear_fast(float3 color_srgb)
{
    return select((color_srgb < 0.04045), color_srgb / 12.92, -7.43605 * color_srgb - 31.24297 * sqrt(-0.53792 * color_srgb + 1.279924) + 35.34864);
}

// -----------------------------------------------------------------------------
// Rec.709

float3 linear_to_rec709(float3 color_linear)
{
    return select((color_linear < 0.0181), 4.5 * color_linear, 1.0993 * pow(color_linear, 0.45) - 0.0993);
}

float3 rec709_to_linear(float3 color_rec709)
{
    return select((color_rec709 < 0.08145), color_rec709 / 4.5, pow((color_rec709 + 0.0993) / 1.0993, 1.0 / 0.45));
}

// -----------------------------------------------------------------------------
// ST.2084 (or PQ)

static const float PQ_MAX_NITS = 10000.0;
static const float PQ_M1 = 2610.0 / 4096.0 / 4.0;
static const float PQ_M2 = 2523.0 / 4096.0 * 128.0;
static const float PQ_C1 = 3424.0 / 4096.0;
static const float PQ_C2 = 2413.0 / 4096.0 * 32.0;
static const float PQ_C3 = 2392.0 / 4096.0 * 32.0;

float3 linear_to_pq(float3 color_linear)
{
    float3 Lp = pow(color_linear, PQ_M1);
    return pow((PQ_C1 + PQ_C2 * Lp) / (1.0 + PQ_C3 * Lp), PQ_M2);
}

float3 pq_to_linear(float3 color_pq)
{
    float3 Np = pow(color_pq, 1.0 / PQ_M2);
    return pow(max(Np - PQ_C1, 0.0) / (PQ_C2 - PQ_C3 * Np), 1.0 / PQ_M1);
}

// -----------------------------------------------------------------------------
// Miscellaneous

float3 linear_to_gamma22(float3 color_linear)
{
    return pow(color_linear, 1.0 / 2.2);
}

float3 gamma22_to_linear(float3 color_g22)
{
    return pow(color_g22, 2.2);
}

// -----------------------------------------------------------------------------

float3 linear_to_gamma24(float3 color_linear)
{
    return pow(color_linear, 1.0 / 2.4);
}

float3 gamma24_to_linear(float3 color_g24)
{
    return pow(color_g24, 2.4);
}

#endif
