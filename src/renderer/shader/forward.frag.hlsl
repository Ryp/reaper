#include "lib/base.hlsl"

#include "lib/lighting.hlsl"
#include "lib/normal_mapping.hlsl"

#include "forward.share.hlsl"
#include "mesh_instance.share.hlsl"
#include "lighting.share.hlsl"

static const uint debug_mode_none = 0;
static const uint debug_mode_albedo = 1;
static const uint debug_mode_normal = 2;
static const uint debug_mode_roughness = 3;
static const uint debug_mode_metallic = 4;
static const uint debug_mode_ao = 5;

// https://github.com/microsoft/DirectXShaderCompiler/issues/2957
#if defined(_DXC)
VK_CONSTANT(0) const uint spec_debug_mode = 0;
#else
VK_CONSTANT(0) const uint spec_debug_mode = debug_mode_none;
#endif
VK_CONSTANT(1) const bool spec_debug_enable_shadows = true;

VK_BINDING(0, 0) ConstantBuffer<ForwardPassParams> pass_params;
VK_BINDING(0, 7) StructuredBuffer<PointLightProperties> point_lights;
VK_BINDING(0, 8) SamplerComparisonState shadow_map_sampler;
VK_BINDING(0, 9) Texture2D<float> shadow_map_array[ShadowMapMaxCount];

VK_BINDING(1, 0) SamplerState diffuse_map_sampler;
VK_BINDING(1, 1) Texture2D<float3> material_maps[MaterialTextureMaxCount];

struct PS_INPUT
{
    float4 PositionCS   : SV_Position;
    float3 PositionVS   : TEXCOORD0;
    float3 NormalVS     : TEXCOORD1;
    float3 tangent_vs   : TEXCOORD2;
    nointerpolation float bitangent_sign : TEXCOORD3;
    float2 UV           : TEXCOORD4;
    float3 PositionWS   : TEXCOORD5;
    nointerpolation uint albedo_texture_index : TEXCOORD6;
    nointerpolation uint roughness_texture_index : TEXCOORD7;
    nointerpolation uint normal_texture_index : TEXCOORD8;
    nointerpolation uint ao_texture_index : TEXCOORD9;
};

struct PS_OUTPUT
{
    float3 color : SV_Target0;
};

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    const float3 view_direction_vs = -normalize(input.PositionVS);
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

    LightOutput lighting_accum = (LightOutput)0;

    for (uint i = 0; i < pass_params.point_light_count; i++)
    {
        const PointLightProperties point_light = point_lights[i];

        const LightOutput lighting = shade_point_light(point_light, material, input.PositionVS, view_direction_vs);

        float shadow_term = 1.0;
        if (point_light.shadow_map_index != InvalidShadowMapIndex && spec_debug_enable_shadows)
        {
            // NOTE: shadow_map_index MUST be uniform
            shadow_term = sample_shadow_map(shadow_map_array[point_light.shadow_map_index], shadow_map_sampler, point_light.light_ws_to_cs, input.PositionWS);
        }

        lighting_accum.diffuse += lighting.diffuse * shadow_term;
        lighting_accum.specular += lighting.specular * shadow_term;
    }

    float3 ambient_term = 0.15f;
    lighting_accum.diffuse += ambient_term;

    float3 lighting_sum = material.albedo * (lighting_accum.diffuse + lighting_accum.specular);

    if (spec_debug_mode == debug_mode_albedo)
        lighting_sum = material.albedo;
    else if (spec_debug_mode == debug_mode_normal)
        lighting_sum = material.normal_vs * 0.5 + 0.5;
    else if (spec_debug_mode == debug_mode_roughness)
        lighting_sum = material.roughness;
    else if (spec_debug_mode == debug_mode_metallic)
        lighting_sum = material.f0;
    else if (spec_debug_mode == debug_mode_ao)
        lighting_sum = material.ao;

    output.color = lighting_sum;
}
