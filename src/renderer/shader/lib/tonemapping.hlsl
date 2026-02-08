////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_TONEMAPPING_INCLUDED
#define LIB_TONEMAPPING_INCLUDED

// Mainly for reference, it probably shouldn't be used
// http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 tonemapping_uncharted2(float3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;

    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 tonemapping_filmic_aces(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

// http://cdn2.gran-turismo.com/data/www/pdi_publications/PracticalHDRandWCGinGTS_20181222.pdf
// https://www.desmos.com/calculator/gslcdxvipg
// https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
struct TonemapGranTurismoConstants
{
    float linear_section_start;
    float contrast;
    float toe_tightness;
    float black_brightness;
    float maximum_brightness;
    float linear_section_length;
};

TonemapGranTurismoConstants default_tonemap_gran_turismo(float brightness_min, float brightness_max)
{
    TonemapGranTurismoConstants consts;

    consts.linear_section_start = 100.0;
    consts.contrast = 1.0;
    consts.toe_tightness = 1.22;
    consts.black_brightness = brightness_min;
    consts.maximum_brightness = brightness_max;
    consts.linear_section_length = 0.9;

    return consts;
};

float tonemapping_gran_turismo_2017(float x, float brightness_min, float brightness_max)
{
    TonemapGranTurismoConstants consts = default_tonemap_gran_turismo(brightness_min, brightness_max);

    // FIXME this code needs rewriting!
    float m = consts.linear_section_start;
    float a = consts.contrast;
    float c = consts.toe_tightness;
    float b = consts.black_brightness;
    float P = consts.maximum_brightness;
    float l = consts.linear_section_length;

    float l0 = ((P - m) * l) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float L = m + a * (x - m);
    float T = m * pow(x / m, c) + b;
    float S = P - (P - S1) * exp(-C2 * (x - S0) / P);
    float w0 = 1.0 - smoothstep(0.0, m, x);
    float w2 = (x < m + l) ? 0.0 : 1.0; // FIXME step?
    float w1 = 1.0 - w0 - w2;
    return float(T * w0 + L * w1 + S * w2);
}

static const float ToneMappingStopsStart = -16.f;
static const float ToneMappingStopsEnd = 16.f;
static const float ToneMappingStopsRange = ToneMappingStopsEnd - ToneMappingStopsStart;
static const float ToneMappingStopsOffset = -ToneMappingStopsStart;

float remap_lut_input_range(float x)
{
    return (log2(x) + ToneMappingStopsOffset) / ToneMappingStopsRange;
}

float remap_lut_input_range_inverse(float x)
{
    return exp2(x * ToneMappingStopsRange - ToneMappingStopsOffset);
}

#endif
