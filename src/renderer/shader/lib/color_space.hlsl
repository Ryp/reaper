////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_COLOR_SPACE_INCLUDED
#define LIB_COLOR_SPACE_INCLUDED

// Rec.709
float luma_rec709(float3 color_rec709)
{
    // NOTE: BT.709-2 introduces slightly different luma coefficients
    const float3 luma_coeff_rec709_2 = float3(0.2125, 0.7154, 0.0721);

    return dot(color_rec709, luma_coeff_rec709_2);
}

// -----------------------------------------------------------------------------
// sRGB
// NOTE: sRGB and Rec.709 share the same primaries and white point (D65)

float3 rec709_to_srgb(float3 color_rec709)
{
    return color_rec709;
}

float3 srgb_to_rec709(float3 color_srgb)
{
    return color_srgb;
}

// -----------------------------------------------------------------------------
// Rec.2020

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

float luma_rec2020(float3 color_rec2020)
{
    const float3 luma_coeff_rec2020 = float3(0.2627, 0.6780, 0.0593);

    return dot(color_rec2020, luma_coeff_rec2020);
}

// -----------------------------------------------------------------------------
// DCI-P3

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

#endif
