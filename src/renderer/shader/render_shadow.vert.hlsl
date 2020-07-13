#include "lib/base.hlsl"

#include "share/shadow_map_pass.hlsl"

VK_BINDING(0, 0) ConstantBuffer<ShadowMapPassParams> pass_params;
VK_BINDING(1, 0) StructuredBuffer<ShadowMapInstanceParams> instance_params;

struct VS_INPUT
{
    float3 PositionMS : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 PositionCS : SV_Position;
};

VS_OUTPUT main(VS_INPUT input, uint instance_id : SV_InstanceID)
{
    const float3 positionMS = input.PositionMS;
    const float3 positionWS = mul(instance_params[instance_id].model, float4(positionMS, 1.0));
    const float4 positionCS = mul(pass_params.viewProj, float4(positionWS, 1.0));

    VS_OUTPUT output;

    output.PositionCS = positionCS;

    return output;
}
