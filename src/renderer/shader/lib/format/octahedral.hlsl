////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_FORMAT_OCTAHEDRAL_INCLUDED
#define LIB_FORMAT_OCTAHEDRAL_INCLUDED

#include "snorm.hlsl"

////////////////////////////////////////////////////////////////////////////////
// Octahedral encoding for unit vectors lying on the hemisphere

// 2014 - A Survey of Efﬁcient Representations for Independent Unit Vectors
// https://jcgt.org/published/0003/02/01/
//
// Assume normalized input on +Z hemisphere.
// Output is on [-1, 1].
float2 encode_normal_hemi_octahedral(float3 normal)
{
    // Project the hemisphere onto the hemi-octahedron,
    // and then into the xy plane
    float2 encoded_normal = normal.xy / (abs(normal.x) + abs(normal.y) + normal.z);

    // Rotate and scale the center diamond to the unit square
    return float2(encoded_normal.x + encoded_normal.y, encoded_normal.x - encoded_normal.y);
}

float3 decode_normal_hemi_octahedral(float2 encoded_normal)
{
    // Rotate and scale the unit square back to the center diamond
    float2 temp = float2(encoded_normal.x + encoded_normal.y, encoded_normal.x - encoded_normal.y) * 0.5;
    float3 normal = float3(temp, 1.0 - abs(temp.x) - abs(temp.y));

    return normalize(normal);
}

// Wrapper to that we can get a packed snorm as output directly
//
// Assume normalized input on +Z hemisphere.
uint encode_normal_hemi_octahedral(float3 normal, uint bits_per_channel)
{
    float2 encoded_normal = encode_normal_hemi_octahedral(normal);

    return rg32_float_to_snorm_generic(encoded_normal, bits_per_channel);
}

float3 decode_normal_hemi_octahedral(uint encoded_normal, uint bits_per_channel)
{
    float2 encoded_normal_float2 = rg_snorm_generic_to_rg32_float(encoded_normal, bits_per_channel);

    return decode_normal_hemi_octahedral(encoded_normal_float2);
}

////////////////////////////////////////////////////////////////////////////////
// Octahedral encoding for unit vectors lying on the full sphere

// 2017 - Signed Octahedron Normal Encoding
// https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
//
// Assume normalized input on sphere.
// Output is on [0, 1] where Z is binary
float3 encode_normal_octahedral_white(float3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);

    float3 encoded_normal;

    encoded_normal.y = normal.y *  0.5 + 0.5;
    encoded_normal.x = normal.x *  0.5 + encoded_normal.y;
    encoded_normal.y = normal.x * -0.5 + encoded_normal.y;

    // Trick to make Z the sign bit (0.0 for negative and 1.0 for positive)
    encoded_normal.z = saturate(normal.z * FLT_MAX);

    return encoded_normal;
}

float3 decode_normal_octahedral_white(float3 encoded_normal)
{
    float3 normal;

    normal.x = encoded_normal.x - encoded_normal.y;
    normal.y = encoded_normal.x + encoded_normal.y - 1.0;

    // Since Z was encoded as a binary info we can extract the sign from it
    normal.z = encoded_normal.z * 2.0 - 1.0;
    normal.z = normal.z * (1.0 - abs(normal.x) - abs(normal.y));

    return normalize(normal);
}

// 2014 - https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
float2 oct_wrap(float2 v)
{
    return (1.0 - abs(v.yx)) * select(v.xy >= 0.0, 1.0, -1.0);
}

// Output is in [-1, 1]
float2 encode_normal_octahedral_stubbe(float3 n)
{
    n /= abs(n.x) + abs(n.y) + abs(n.z);

    n.xy = n.z >= 0.0 ? n.xy : oct_wrap(n.xy);

    return n.xy;
}

float3 decode_normal_octahedral_stubbe(float2 f)
{
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = saturate( -n.z );
    n.xy += select(n.xy >= 0.0, -t, t);

    return normalize(n);
}

#endif
