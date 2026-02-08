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

struct TonemapGranTurismo2025Constants
{
   float peakIntensity_;
   float alpha_;
   float midPoint_;
   float linearSection_;
   float toeStrength_;
   float kA_;
   float kB_;
   float kC_;
};

TonemapGranTurismo2025Constants compute_tonemap_gran_turismo_constants(float target_luminance_nits, float brightness_min, float brightness_max)
{
    TonemapGranTurismo2025Constants consts;
    consts.peakIntensity_ = target_luminance_nits;
    consts.alpha_         = 0.25f;
    consts.midPoint_      = 0.538f;
    consts.linearSection_ = 0.444f;
    consts.toeStrength_   = 1.280f;

    // Pre-compute constants for the shoulder region.
    float k = (consts.linearSection_ - 1.0f) / (consts.alpha_ - 1.0f);
    consts.kA_     = consts.peakIntensity_ * consts.linearSection_ + consts.peakIntensity_ * k;
    consts.kB_     = -consts.peakIntensity_ * k * exp(consts.linearSection_ / k);
    consts.kC_     = -1.0f / (k * consts.peakIntensity_);

    return consts;
}

float tonemapping_gran_turismo_2025(float x, float brightness_min, float brightness_max)
{
    float target_luminance_nits = 250.f;
    TonemapGranTurismo2025Constants consts = compute_tonemap_gran_turismo_constants(target_luminance_nits, brightness_min, brightness_max);

    if (x < 0.f)
    {
        return 0.f;
    }

    float weightLinear = smoothstep(x, 0.f, consts.midPoint_);
    float weightToe    = 1.f - weightLinear;

    // Shoulder mapping for highlights.
    float shoulder = consts.kA_ + consts.kB_ * exp(x * consts.kC_);

    if (x < consts.linearSection_ * consts.peakIntensity_)
    {
        float toeMapped = consts.midPoint_ * pow(x / consts.midPoint_, consts.toeStrength_);
        return weightToe * toeMapped + weightLinear * x;
    }
    else
    {
        return shoulder;
    }
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
