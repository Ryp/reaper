////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_NORMAL_MAPPING_INCLUDED
#define LIB_NORMAL_MAPPING_INCLUDED

float3 decode_normal_map(float3 encoded_normal)
{
    return normalize(encoded_normal * 2.f - 1.f);
}

// NOTE:
// Assumes normal_map_tangent_space, tangent_vs and geometric_normal_vs are normalized.
// Returns a normalized normal in view space.
float3 compute_tangent_space_normal_map(float3 geometric_normal_vs, float3 tangent_vs, float bitangent_sign, float3 normal_map_tangent_space)
{
    const float3 bitangent_vs = cross(geometric_normal_vs, tangent_vs) * bitangent_sign; // FIXME Normalize?
    const float3x3 tbn_matrix = float3x3(tangent_vs, bitangent_vs, geometric_normal_vs); // FIXME Ortho-normalize?
    float3 normal_vs = mul(transpose(tbn_matrix), normal_map_tangent_space);

    // FIXME Since we encode the GBuffer normal with hemi octahedral, we clamp the normal to stay consistent with forward
    normal_vs.z = saturate(normal_vs.z);

    return normalize(normal_vs);
}

#endif
