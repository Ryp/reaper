#include "lib/base.hlsl"

#include "lib/brdf.hlsl"
#include "share/draw.hlsl"

static const uint debug_mode_none = 0;
static const uint debug_mode_normals = 1;
static const uint debug_mode_uv = 2;
static const uint debug_mode_pos_vs = 3;

VK_CONSTANT(1) const uint spec_debug_mode = debug_mode_none;

VK_BINDING(0, 0) ConstantBuffer<DrawPassParams> pass_params;

struct PS_INPUT
{
    float4 PositionCS : SV_Position;
    float3 PositionVS : TEXCOORD0;
    float3 NormalVS : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
};

PS_OUTPUT main(PS_INPUT input)
{
    const float3 object_to_light_vs = pass_params.light_position_vs - input.PositionVS;
    const float3 light_direction_vs = normalize(object_to_light_vs);

    const float light_distance_sq = dot(object_to_light_vs, object_to_light_vs);
    const float light_distance_fade = rcp(1.0 + light_distance_sq);

    const float3 normal_vs = normalize(input.NormalVS);

    const float NdotL = saturate(dot(normal_vs, light_direction_vs));

    const float3 light_color = float3(0.8, 0.5, 0.2);
    const float light_intensity = 8.f;

    const float3 light_radiance = light_color * light_intensity * light_distance_fade;

    const float3 object_albedo = float3(0.9, 0.9, 0.9);

    StandardMaterial material;
    material.roughness = 0.5;
    material.f0 = 0.1;

    const float3 view_direction_vs = -normalize(input.PositionVS);

    const float3 diffuse = light_radiance * NdotL;
    const float3 specular = light_radiance * specular_brdf(material, normal_vs, view_direction_vs, light_direction_vs);

    PS_OUTPUT output;

    if (spec_debug_mode == debug_mode_none)
        output.color = float4(object_albedo * (diffuse + specular), 1.0);
    else if (spec_debug_mode == debug_mode_normals)
        output.color = float4(normal_vs * 0.5 + 0.5, 1.0);
    else if (spec_debug_mode == debug_mode_uv)
        output.color = float4(input.UV, 0.0, 1.0);
    else if (spec_debug_mode == debug_mode_pos_vs)
        output.color = float4(input.PositionVS, 1.0);

    return output;
}
