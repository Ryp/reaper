#include "lib/base.hlsl"

#include "lib/brdf.hlsl"
#include "share/draw.hlsl"

static const uint debug_mode_none = 0;
static const uint debug_mode_normals = 1;
static const uint debug_mode_uv = 2;
static const uint debug_mode_pos_vs = 3;

VK_CONSTANT(1) const uint spec_debug_mode = debug_mode_none;

VK_BINDING(0, 0) ConstantBuffer<DrawPassParams> pass_params;

VK_BINDING(2, 0) Texture2D<float> t_shadow_map;
VK_BINDING(3, 0) SamplerComparisonState shadow_map_sampler;

struct PS_INPUT
{
    float4 PositionCS : SV_Position;
    float3 PositionVS : TEXCOORD0;
    float3 NormalVS : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float3 PositionWS : TEXCOORD3;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
};

struct t_light_output
{
    float3 diffuse;
    float3 specular;
};

t_light_output shade_point_light(
    PointLightProperties point_light, StandardMaterial material,
    float3 object_position_vs, float3 object_normal_vs, float3 view_direction_vs)
{
    const float3 object_to_light_vs = point_light.position_vs - object_position_vs;
    const float3 light_direction_vs = normalize(object_to_light_vs);

    const float light_distance_sq = dot(object_to_light_vs, object_to_light_vs);
    const float light_distance_fade = rcp(1.0 + light_distance_sq);

    const float NdotL = saturate(dot(object_normal_vs, light_direction_vs));

    const float3 light_incoming_radiance = point_light.color * point_light.intensity * light_distance_fade;

    t_light_output output;
    output.diffuse = light_incoming_radiance * NdotL;
    output.specular = light_incoming_radiance * specular_brdf(material, object_normal_vs, view_direction_vs, light_direction_vs);

    return output;
}

float sample_shadow_map(float4x4 light_transform_ws_to_cs, float3 object_position_ws)
{
    const float4 position_shadow_map_cs = mul(light_transform_ws_to_cs, float4(object_position_ws, 1.0));
    const float3 position_shadow_map_ndc = position_shadow_map_cs.xyz / position_shadow_map_cs.w;
    const float2 position_shadow_map_uv = ndc_to_uv(position_shadow_map_ndc.xy);

    const float shadow_depth_bias = 0.001;
    const float shadow_pcf = t_shadow_map.SampleCmp(shadow_map_sampler, position_shadow_map_uv, position_shadow_map_ndc.z + shadow_depth_bias);

    return shadow_pcf;
}

PS_OUTPUT main(PS_INPUT input)
{
    const float3 view_direction_vs = -normalize(input.PositionVS);
    const float3 normal_vs = normalize(input.NormalVS);

    StandardMaterial material;
    material.albedo = float3(0.9, 0.9, 0.9);
    material.roughness = 0.5;
    material.f0 = 0.1;

    float3 shaded_color = 0.0;

    for (uint i = 0; i < PointLightCount; i++)
    {
        const PointLightProperties point_light = pass_params.point_light[i];

        const t_light_output lighting = shade_point_light(point_light, material, input.PositionVS, normal_vs, view_direction_vs);
        const float shadow_term = sample_shadow_map(point_light.light_ws_to_cs, input.PositionWS);

        shaded_color += material.albedo * (lighting.diffuse + lighting.specular) * shadow_term;
    }

    PS_OUTPUT output;

    if (spec_debug_mode == debug_mode_none)
        output.color = float4(shaded_color, 1.0);
    else if (spec_debug_mode == debug_mode_normals)
        output.color = float4(normal_vs * 0.5 + 0.5, 1.0);
    else if (spec_debug_mode == debug_mode_uv)
        output.color = float4(input.UV, 0.0, 1.0);
    else if (spec_debug_mode == debug_mode_pos_vs)
        output.color = float4(input.PositionVS, 1.0);

    return output;
}
