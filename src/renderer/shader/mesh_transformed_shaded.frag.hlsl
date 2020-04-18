#include "lib/base.hlsl"

#include "lib/brdf.hlsl"
#include "share/draw.hlsl"

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

    const float3 normalVS = normalize(input.NormalVS);

    const float NdotL = saturate(dot(normalVS, light_direction_vs));

    const float3 object_albedo = float3(0.9, 0.9, 0.9);
    const float3 light_color = float3(0.8, 0.5, 0.2);
    const float light_intensity = 8.f;

    const float3 lambert_diffuse = object_albedo * light_color * light_intensity * NdotL * light_distance_fade;

    PS_OUTPUT output;

    output.color = float4(lambert_diffuse, 1.0);

    return output;
}
