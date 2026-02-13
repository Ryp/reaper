#include "lib/base.hlsl"

#include "lib/lighting.hlsl"
#include "lib/brdf.hlsl"
#include "lib/normal_mapping.hlsl"
#include "gbuffer/gbuffer.hlsl"

#include "mesh_instance.share.hlsl"
#include "mesh_material.share.hlsl"
#include "gbuffer_write_opaque.share.hlsl"

VK_BINDING(0, Slot_mesh_materials) StructuredBuffer<MeshMaterial> mesh_materials;
VK_BINDING(0, Slot_diffuse_map_sampler) SamplerState diffuse_map_sampler;
VK_BINDING(0, Slot_material_maps) Texture2D<float3> material_maps[MaterialTextureMaxCount];

struct PS_INPUT
{
    float4 PositionCS   : SV_Position;
    float3 NormalVS     : TEXCOORD0;
    float3 tangent_vs   : TEXCOORD1;
    nointerpolation float bitangent_sign : TEXCOORD2;
    float2 UV           : TEXCOORD3;
    nointerpolation uint material_index : TEXCOORD4;
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

    MeshMaterial mesh_material = mesh_materials[input.material_index];

    float3 normal_map_normal = decode_normal_map(material_maps[mesh_material.normal_texture_index].Sample(diffuse_map_sampler, input.UV).xyz);
    float3 normal_vs = compute_tangent_space_normal_map(geometric_normal_vs, tangent_vs, input.bitangent_sign, normal_map_normal);

    StandardMaterial material;
    material.albedo = material_maps[mesh_material.albedo_texture_index].Sample(diffuse_map_sampler, input.UV).rgb;
    material.normal_vs = normal_vs;
    material.roughness = material_maps[mesh_material.roughness_texture_index].Sample(diffuse_map_sampler, input.UV).z;
    material.f0 = material_maps[mesh_material.roughness_texture_index].Sample(diffuse_map_sampler, input.UV).y;
    material.ao = material_maps[mesh_material.ao_texture_index].Sample(diffuse_map_sampler, input.UV).x;

    const GBuffer gbuffer = gbuffer_from_standard_material(material);
    const GBufferRaw gbuffer_raw = encode_gbuffer(gbuffer);

    output.rt0 = gbuffer_raw.rt0;
    output.rt1 = gbuffer_raw.rt1;
}
