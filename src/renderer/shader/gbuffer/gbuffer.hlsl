////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef GBUFFER_INCLUDED
#define GBUFFER_INCLUDED

#include "lib/format/unorm.hlsl"
#include "lib/format/octahedral.hlsl"
#include "lib/format/bitfield.hlsl"
#include "lib/tranfer_functions.hlsl"

#include "material/standard.hlsl"

#define GBuffer0Type uint
#define GBuffer1Type uint

struct GBufferRaw
{
    GBuffer0Type rt0;
    GBuffer1Type rt1;
};

static const uint RT1_Normal_Offset = 0;
static const uint RT1_Normal_Bits = 20; // NOTE: Must be even because there's two channels
static const uint RT1_FZero_Offset = RT1_Normal_Offset + RT1_Normal_Bits;
static const uint RT1_FZero_Bits = 6;
static const uint RT1_AO_Offset = RT1_FZero_Offset + RT1_FZero_Bits;
static const uint RT1_AO_Bits = 6;

struct GBuffer
{
    float3 albedo;
    float roughness;
    float3 normal_vs;
    float f0;
    float ao;
};

GBuffer default_gbuffer()
{
    GBuffer gbuffer;

    gbuffer.albedo = float3(0.0, 0.0, 0.0);
    gbuffer.roughness = 1.0;
    gbuffer.normal_vs = float3(0.0, 0.0, 1.0);
    gbuffer.f0 = 0.0;
    gbuffer.ao = 1.0;

    return gbuffer;
}

// You're responsible to feed well-formed data!
// FIXME use safe versions that clamp input?
GBufferRaw encode_gbuffer(GBuffer gbuffer)
{
    GBufferRaw gbuffer_raw;

    gbuffer_raw.rt0 = rgba32_float_to_rgba8_unorm(float4(linear_to_srgb_fast(gbuffer.albedo), gbuffer.roughness));

    gbuffer_raw.rt1 = encode_normal_hemi_octahedral(gbuffer.normal_vs, RT1_Normal_Bits / 2) << RT1_Normal_Offset |
         r32_float_to_unorm_generic(gbuffer.f0, RT1_FZero_Bits) << RT1_FZero_Offset |
         r32_float_to_unorm_generic(gbuffer.ao, RT1_AO_Bits) << RT1_AO_Offset;

    return gbuffer_raw;
}

GBuffer decode_gbuffer(GBufferRaw gbuffer_raw)
{
    GBuffer gbuffer;

    const float4 packed_albedo_roughness = rgba8_unorm_to_rgba32_float(gbuffer_raw.rt0);

    gbuffer.albedo = srgb_to_linear_fast(packed_albedo_roughness.xyz);
    gbuffer.roughness = packed_albedo_roughness.w;

    const uint packed_normal_f0_ao = gbuffer_raw.rt1;

    gbuffer.normal_vs = decode_normal_hemi_octahedral(packed_normal_f0_ao >> RT1_Normal_Offset, RT1_Normal_Bits / 2);
    gbuffer.f0 = r_unorm_generic_to_r32_float(bitfield_extract(packed_normal_f0_ao, RT1_FZero_Offset, RT1_FZero_Bits), RT1_FZero_Bits);
    gbuffer.ao = r_unorm_generic_to_r32_float(bitfield_extract(packed_normal_f0_ao, RT1_AO_Offset, RT1_AO_Bits), RT1_AO_Bits);

    return gbuffer;
}

GBuffer gbuffer_from_standard_material(StandardMaterial material)
{
    GBuffer gbuffer;

    gbuffer.albedo = material.albedo;
    gbuffer.roughness = material.roughness;
    gbuffer.normal_vs = material.normal_vs;
    gbuffer.f0 = material.f0;
    gbuffer.ao = material.ao;

    return gbuffer;
}

StandardMaterial standard_material_from_gbuffer(GBuffer gbuffer)
{
    StandardMaterial material;

    material.albedo = gbuffer.albedo;
    material.roughness = gbuffer.roughness;
    material.normal_vs = gbuffer.normal_vs;
    material.f0 = gbuffer.f0;
    material.ao = gbuffer.ao;

    return material;
}

#endif
