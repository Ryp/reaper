////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_COLOR_SPACE_INCLUDED
#define LIB_COLOR_SPACE_INCLUDED

// -----------------------------------------------------------------------------
// Color Spaces
// -----------------------------------------------------------------------------
// NOTE: sRGB and rec709 share the same primaries.

float3 rec709_to_srgb(float3 color_rec709)
{
    return color_rec709; // Primaries are the same
}

float3 srgb_to_rec709(float3 color_srgb)
{
    return color_srgb; // Primaries are the same
}

// -----------------------------------------------------------------------------

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

float3 rec2020_to_rec709(float3 color_rec2020)
{
    static const float3x3 rec2020_to_rec709_matrix =
    {
        1.660496, -0.587656, -0.072840,
        -0.124547, 1.132895, -0.008348,
        -0.018154, -0.100597, 1.118751
    };

    return mul(rec2020_to_rec709_matrix, color_rec2020);
}

// -----------------------------------------------------------------------------

float3 rec709_to_dci_p3(float3 color_rec709)
{
    static const float3x3 rec709_to_dci_p3_matrix =
    {
        0.822458, 0.177542, 0.000000,
        0.033193, 0.966807, 0.000000,
        0.017085, 0.072410, 0.910505
    };

    return mul(rec709_to_dci_p3_matrix, color_rec709);
}

float3 dci_p3_to_rec709(float3 color_dci_p3)
{
    static const float3x3 dci_p3_to_rec709_matrix =
    {
        1.224947, -0.224947, 0.000000,
        -0.042056, 1.042056, 0.000000,
        -0.019641, -0.078651, 1.098291
    };

    return mul(dci_p3_to_rec709_matrix, color_dci_p3);
}

// -----------------------------------------------------------------------------
// EOTFs
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

// -----------------------------------------------------------------------------

float3 srgb_eotf(float3 color)
{
    // Looks like the gamma 2.2 function
    return color < 0.0031308 ? 12.92 * color : 1.055 * pow(color, 1.0 / 2.4) - 0.055;
}

float3 srgb_eotf_inverse(float3 x)
{
    // Looks like the gamma 2.2 inverse function
    return x < 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

// Replace pow() with cheaper sqrt()
// Error < 0.4%
float3 srgb_eotf_fast(float3 color)
{
    return color < 0.0031308 ? 12.92 * color : 1.13005 * sqrt(color - 0.00228) - 0.13448 * color + 0.005719;
}

float3 srgb_eotf_inverse_fast(float3 x)
{
    return x < 0.04045 ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
}

// -----------------------------------------------------------------------------

float3 rec709_eotf(float3 color)
{
    return color < 0.0181 ? 4.5 * color : 1.0993 * pow(color, 0.45) - 0.0993;
}

float3 rec709_eotf_inverse(float3 x)
{
    return x < 0.08145 ? x / 4.5 : pow((x + 0.0993) / 1.0993, 1.0 / 0.45);
}

// -----------------------------------------------------------------------------

float3 pq_eotf(float3 color)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;

    float3 Lp = pow(color, m1);
    return pow((c1 + c2 * Lp) / (1 + c3 * Lp), m2);
}

float3 pq_eotf_inverse(float3 pq_eotf_color)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;

    float3 Np = pow(pq_eotf_color, 1.0 / m2);
    return pow(max(Np - c1, 0.0) / (c2 - c3 * Np), 1 / m1);
}

// -----------------------------------------------------------------------------

float luma_rec709(float3 color_rec709)
{
    // NOTE: BT.709-2 introduces slightly different luma coefficients
    // const float3 luma_coeff_rec709 = float3(0.2125, 0.7154, 0.0721);
    const float3 luma_coeff_rec709 = float3(0.2126, 0.7152, 0.0722);

    return dot(color_rec709, luma_coeff_rec709);
}

float luma_rec2020(float3 color_rec2020)
{
    const float3 luma_coeff_rec2020 = float3(0.2627, 0.6780, 0.0593);

    return dot(color_rec2020, luma_coeff_rec2020);
}

#endif
