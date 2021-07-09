////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#define LIB_TONEMAPPING_INCLUDED
#ifndef LIB_TONEMAPPING_INCLUDED

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

#endif
