#include "lib/base.hlsl"

#include "lib/lighting.hlsl"
#include "lib/brdf.hlsl"
#include "gbuffer/gbuffer.hlsl"

#include "forward.share.hlsl" // FIXME

VK_BINDING(0, 5) SamplerState diffuse_map_sampler;
VK_BINDING(0, 6) Texture2D<float3> diffuse_maps[DiffuseMapMaxCount];

struct PS_INPUT
{
    float4 PositionCS   : SV_Position;
    float3 NormalVS     : TEXCOORD0;
    float2 UV           : TEXCOORD1;
    nointerpolation uint texture_index : TEXCOORD2;
};

struct PS_OUTPUT
{
    GBuffer0Type rt0 : SV_Target0;
    GBuffer1Type rt1 : SV_Target1;
};

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    StandardMaterial material;
    material.albedo = diffuse_maps[input.texture_index].Sample(diffuse_map_sampler, input.UV);
    material.roughness = 0.5;
    material.f0 = 0.1;

    GBuffer gbuffer;
    gbuffer.albedo = material.albedo;
    gbuffer.roughness = material.roughness;
    gbuffer.f0 = material.f0;
    gbuffer.normal_vs = normalize(input.NormalVS);

    const GBufferRaw gbuffer_raw = encode_gbuffer(gbuffer);

    output.rt0 = gbuffer_raw.rt0;
    output.rt1 = gbuffer_raw.rt1;
}
