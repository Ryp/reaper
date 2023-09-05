#include "lib/base.hlsl"

#include "lib/lighting.hlsl"
#include "lib/brdf.hlsl"
#include "lib/normal_mapping.hlsl"
#include "gbuffer/gbuffer.hlsl"

#include "mesh_instance.share.hlsl"
#include "gbuffer_write_opaque.share.hlsl"

VK_BINDING(0, 5) SamplerState diffuse_map_sampler;
VK_BINDING(0, 6) Texture2D<float3> material_maps[MaterialTextureMaxCount];

struct PS_INPUT
{
    float4 PositionCS   : SV_Position;
    float3 NormalVS     : TEXCOORD0;
    float3 tangent_vs   : TEXCOORD1;
    nointerpolation float bitangent_sign : TEXCOORD2;
    float2 UV           : TEXCOORD3;
    nointerpolation uint albedo_texture_index : TEXCOORD4;
    nointerpolation uint roughness_texture_index : TEXCOORD5;
    nointerpolation uint normal_texture_index : TEXCOORD6;
    nointerpolation uint ao_texture_index : TEXCOORD7;
};

struct PS_OUTPUT
{
    GBuffer0Type rt0 : SV_Target0;
    GBuffer1Type rt1 : SV_Target1;
};

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    const float3 geometric_normal_vs = normalize(input.NormalVS);
    const float3 tangent_vs = normalize(input.tangent_vs); // FIXME Normalize?

    float3 normal_map_normal = decode_normal_map(material_maps[input.normal_texture_index].Sample(diffuse_map_sampler, input.UV).xyz);

    float3 normal_vs = compute_tangent_space_normal_map(geometric_normal_vs, tangent_vs, input.bitangent_sign, normal_map_normal);

    StandardMaterial material;
    material.albedo = material_maps[input.albedo_texture_index].Sample(diffuse_map_sampler, input.UV).rgb;
    material.normal_vs = normal_vs;
    material.roughness = material_maps[input.roughness_texture_index].Sample(diffuse_map_sampler, input.UV).z;
    material.f0 = material_maps[input.roughness_texture_index].Sample(diffuse_map_sampler, input.UV).y;
    material.ao = material_maps[input.ao_texture_index].Sample(diffuse_map_sampler, input.UV).x;

    const GBuffer gbuffer = gbuffer_from_standard_material(material);
    const GBufferRaw gbuffer_raw = encode_gbuffer(gbuffer);

    output.rt0 = gbuffer_raw.rt0;
    output.rt1 = gbuffer_raw.rt1;
}
