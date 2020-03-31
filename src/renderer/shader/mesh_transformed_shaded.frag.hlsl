#include "lib/base.hlsl"

#include "lib/brdf.hlsl"
#include "share/draw.hlsl"

VK_BINDING(0, 0) ConstantBuffer<DrawPassParams> pass_params;

struct PS_INPUT
{
    float4 positionCS : SV_Position;
    float3 NormalVS : TEXCOORD0;
    float2 UV : TEXCOORD1;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
};

PS_OUTPUT main(PS_INPUT input)
{
    const float3 lightDirectionVS = float3(0.0, 1.0, 0.0);
    const float3 normalVS = normalize(input.NormalVS);

    const float NdotL = saturate(dot(normalVS, pass_params.light_direction_vs));

    const float3 uvChecker = float3(frac(input.UV * 10.0).xy, 1.0);
    const float3 color = NdotL;

    PS_OUTPUT output;

    output.color = float4(color, 1.0);

    return output;
}
