////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////
//
// All color spaces in this file have the same white point (D65)
// This makes simple matrix-style conversion possible.
//
// NOTE: There's a python script in this repo to regenerate matrix values
// from scratch (convert-rgb-space-xyz.py)
//
// NOTE: Reminder that luma and perceived lightness are NOT the same!

#ifndef LIB_COLOR_SPACE_INCLUDED
#define LIB_COLOR_SPACE_INCLUDED

// -----------------------------------------------------------------------------
// Rec.709
// NOTE: sRGB and Rec.709 share the same primaries and white point (D65)

float3 srgb_to_rec709(float3 color_srgb)
{
    return color_srgb;
}

float3 rec709_to_srgb(float3 color_rec709)
{
    return color_rec709;
}

float luma_rec709(float3 color_rec709)
{
    // NOTE: This standard constant changed at one point and got reverted later
    const float3 luma_coeff_rec709 = float3(0.2125, 0.7154, 0.0721);

    return dot(color_rec709, luma_coeff_rec709);
}

// -----------------------------------------------------------------------------
// sRGB

float luma_srgb(float3 color_srgb)
{
    // NOTE: this is okay to do
    return luma_rec709(color_srgb);
}

// -----------------------------------------------------------------------------
// Rec.2020

float3 srgb_to_rec2020(float3 color_srgb)
{
    static const float3x3 srgb_to_rec2020_matrix =
    {
        0.6274038959, 0.3292830384, 0.0433130657,
        0.0690972894, 0.9195403951, 0.0113623156,
        0.0163914389, 0.0880133079, 0.8955952532,
    };

    return mul(srgb_to_rec2020_matrix, color_srgb);
}

float3 rec2020_to_srgb(float3 color_rec2020)
{
    static const float3x3 rec2020_to_srgb_matrix =
    {
         1.6604910021, -0.5876411388, -0.0728498633,
        -0.1245504745,  1.1328998971, -0.0083494226,
        -0.0181507634, -0.1005788980,  1.1187296614,
    };

    return mul(rec2020_to_srgb_matrix, color_rec2020);
}

float luma_rec2020(float3 color_rec2020)
{
    const float3 luma_coeff_rec2020 = float3(0.2627, 0.6780, 0.0593);

    return dot(color_rec2020, luma_coeff_rec2020);
}

// -----------------------------------------------------------------------------
// Display-P3

float3 srgb_to_display_p3(float3 color_srgb)
{
    static const float3x3 srgb_to_display_p3_matrix =
    {
        0.8224619687, 0.1775380313, 0.0000000000,
        0.0331941989, 0.9668058011, 0.0000000000,
        0.0170826307, 0.0723974407, 0.9105199286,
    };

    return mul(srgb_to_display_p3_matrix, color_srgb);
}

float3 display_p3_to_srgb(float3 color_display_p3)
{
    static const float3x3 display_p3_to_srgb_matrix =
    {
         1.2249401763, -0.2249401763,  0.0000000000,
        -0.0420569547,  1.0420569547, -0.0000000000,
        -0.0196375546, -0.0786360456,  1.0982736001,
    };

    return mul(display_p3_to_srgb_matrix, color_display_p3);
}

// -----------------------------------------------------------------------------
// CIE xy

float3 cie_xy_to_XYZ(float2 cie_xy)
{
    const float2 cie = cie_xy;
    return 0.5 * float3(cie.x / cie.y, 1.0, (1.0 - cie.x - cie.y) / cie.y);
}

float3 cie_xy_to_sRGB(float2 cie_xy)
{
    static const float3x3 XYZ_to_sRGB_matrix =
    {
        3.2409699419, -1.5373831776, -0.4986107603,
       -0.9692436363,  1.8759675015,  0.0415550574,
        0.0556300797, -0.2039769589,  1.0569715142,
    };

    float3 cie_XYZ = cie_xy_to_XYZ(cie_xy);

    return mul(XYZ_to_sRGB_matrix, cie_XYZ);
}

float3 cie_xy_to_rec2020(float2 cie_xy)
{
    static const float3x3 XYZ_to_rec2020_matrix =
    {
        1.7166511880, -0.3556707838, -0.2533662814,
       -0.6666843518,  1.6164812366,  0.0157685458,
        0.0176398574, -0.0427706133,  0.9421031212,
    };

    float3 cie_XYZ = cie_xy_to_XYZ(cie_xy);
    float3 color_rec2020 = mul(XYZ_to_rec2020_matrix, cie_XYZ);
    float maxChannel = max(color_rec2020.r, max(color_rec2020.g, color_rec2020.b));
    return color_rec2020 / maxChannel;
}

#endif
