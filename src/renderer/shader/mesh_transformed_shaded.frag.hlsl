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
VK_BINDING(3, 0) SamplerState shadow_map_sampler;

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

PS_OUTPUT main(PS_INPUT input)
{
    const PointLightProperties point_light = pass_params.point_light;

    const float3 object_to_light_vs = point_light.position_vs - input.PositionVS;
    const float3 light_direction_vs = normalize(object_to_light_vs);

    const float light_distance_sq = dot(object_to_light_vs, object_to_light_vs);
    const float light_distance_fade = rcp(1.0 + light_distance_sq);

    const float3 normal_vs = normalize(input.NormalVS);

    const float NdotL = saturate(dot(normal_vs, light_direction_vs));

    const float3 light_incoming_radiance = point_light.color * point_light.intensity * light_distance_fade;

    const float3 object_albedo = float3(0.9, 0.9, 0.9);

    StandardMaterial material;
    material.roughness = 0.5;
    material.f0 = 0.1;

    const float3 view_direction_vs = -normalize(input.PositionVS);

    const float3 diffuse = light_incoming_radiance * NdotL;
    const float3 specular = light_incoming_radiance * specular_brdf(material, normal_vs, view_direction_vs, light_direction_vs);

    float3 shaded_color = object_albedo * (diffuse + specular);

    // Apply shadow
    {
        const float4 position_shadow_map_cs = mul(point_light.light_ws_to_cs, float4(input.PositionWS, 1.0));
        const float3 position_shadow_map_ndc = position_shadow_map_cs.xyz / position_shadow_map_cs.w;
        const float2 position_shadow_map_uv = ndc_to_uv(position_shadow_map_ndc.xy);

        const float shadow_map_depth_ndc = t_shadow_map.Sample(shadow_map_sampler, position_shadow_map_uv);

        const float3 ambient_color = float3(0.0, 0.0, 0.0);
        const float shadow_depth_bias = 0.001;

        // FIXME handle reverse depth toggle
        if (position_shadow_map_ndc.z + shadow_depth_bias < shadow_map_depth_ndc)
            shaded_color = ambient_color;
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
