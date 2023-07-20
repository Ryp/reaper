////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_GBUFFER_INCLUDED
#define LIB_GBUFFER_INCLUDED

#include "lib/format/unorm.hlsl"
#include "lib/format/octahedral.hlsl"
#include "lib/eotf.hlsl"

#define GBuffer0Type uint
#define GBuffer1Type uint

struct GBufferRaw
{
    GBuffer0Type rt0;
    GBuffer1Type rt1;
};

struct GBuffer
{
    float3 albedo;
    float roughness;
    float3 normal_vs;
    float f0;
};

GBuffer default_gbuffer()
{
    GBuffer gbuffer;

    gbuffer.albedo = float3(0.0, 0.0, 0.0);
    gbuffer.roughness = 1.0;
    gbuffer.normal_vs = float3(0.0, 0.0, 1.0);
    gbuffer.f0 = 0.0;

    return gbuffer;
}

// You're responsible to feed well-formed data!
// FIXME use safe versions that clamp input?
GBufferRaw encode_gbuffer(GBuffer gbuffer)
{
    GBufferRaw gbuffer_raw;

    gbuffer_raw.rt0 = rgba32_float_to_rgba8_unorm(float4(srgb_eotf_fast(gbuffer.albedo), gbuffer.roughness));
    gbuffer_raw.rt1 = encode_normal_hemi_octahedral(gbuffer.normal_vs, 12) | r32_float_to_r8_unorm(gbuffer.f0) << 24;

    return gbuffer_raw;
}

GBuffer decode_gbuffer(GBufferRaw gbuffer_raw)
{
    GBuffer gbuffer;

    const float4 packed_albedo_roughness = rgba8_unorm_to_rgba32_float(gbuffer_raw.rt0);

    gbuffer.albedo = srgb_eotf_inverse_fast(packed_albedo_roughness.xyz);
    gbuffer.roughness = packed_albedo_roughness.w;

    const uint packed_normal_f0 = gbuffer_raw.rt1;

    gbuffer.normal_vs = decode_normal_hemi_octahedral(packed_normal_f0, 12);
    gbuffer.f0 = r8_unorm_to_r32_float(packed_normal_f0 >> 24);

    return gbuffer;
}

#endif
