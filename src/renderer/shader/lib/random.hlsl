////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_RANDOM_INCLUDED
#define LIB_RANDOM_INCLUDED

float rand_neg_1_to_1(float2 n)
{
    return frac(sin(dot(n.xy, float2(12.9898, 78.233)))* 43758.5453);
}

#endif
