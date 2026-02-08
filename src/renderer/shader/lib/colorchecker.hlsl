////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2024 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_COLOR_CHECKER_INCLUDED
#define LIB_COLOR_CHECKER_INCLUDED

// NOTE: Values taken from https://en.wikipedia.org/wiki/ColorChecker
// The colors are sRGB-encoded (non-linear) and normalized

// Row 1: Natural colors
static const float3 mcbeth_01_Dark_skin     = float3(0x73, 0x52, 0x44) / 255.f;
static const float3 mcbeth_02_Light_skin    = float3(0xc2, 0x96, 0x82) / 255.f;
static const float3 mcbeth_03_Blue_sky      = float3(0x62, 0x7a, 0x9d) / 255.f;
static const float3 mcbeth_04_Foliage       = float3(0x57, 0x6c, 0x43) / 255.f;
static const float3 mcbeth_05_Blue_flower   = float3(0x85, 0x80, 0xb1) / 255.f;
static const float3 mcbeth_06_Bluish_green  = float3(0x67, 0xbd, 0xaa) / 255.f;
// Row 2: Miscellaneous colors
static const float3 mcbeth_07_Orange        = float3(0xd6, 0x7e, 0x2c) / 255.f;
static const float3 mcbeth_08_Purplish_blue = float3(0x50, 0x5b, 0xa6) / 255.f;
static const float3 mcbeth_09_Moderate_red  = float3(0xc1, 0x5a, 0x63) / 255.f;
static const float3 mcbeth_10_Purple        = float3(0x5e, 0x3c, 0x6c) / 255.f;
static const float3 mcbeth_11_Yellow_green  = float3(0x9d, 0xbc, 0x40) / 255.f;
static const float3 mcbeth_12_Orange_yellow = float3(0xe0, 0xa3, 0x2e) / 255.f;
// Row 3: Primary and secondary colors
static const float3 mcbeth_13_Blue          = float3(0x38, 0x3d, 0x96) / 255.f;
static const float3 mcbeth_14_Green         = float3(0x46, 0x94, 0x49) / 255.f;
static const float3 mcbeth_15_Red           = float3(0xaf, 0x36, 0x3c) / 255.f;
static const float3 mcbeth_16_Yellow        = float3(0xe7, 0xc7, 0x1f) / 255.f;
static const float3 mcbeth_17_Magenta       = float3(0xbb, 0x56, 0x95) / 255.f;
static const float3 mcbeth_18_Cyan          = float3(0x08, 0x85, 0xa1) / 255.f;
// Row 4: Grayscale colors
static const float3 mcbeth_19_White_N9_5    = float3(0xf3, 0xf3, 0xf3) / 255.f;
static const float3 mcbeth_20_Neutral_N8    = float3(0xc8, 0xc8, 0xc8) / 255.f;
static const float3 mcbeth_21_Neutral_N6_5  = float3(0xa0, 0xa0, 0xa0) / 255.f;
static const float3 mcbeth_22_Neutral_N5    = float3(0x7a, 0x7a, 0x7a) / 255.f;
static const float3 mcbeth_23_Neutral_N3_5  = float3(0x55, 0x55, 0x55) / 255.f;
static const float3 mcbeth_24_Black_N2      = float3(0x34, 0x34, 0x34) / 255.f;

#endif
