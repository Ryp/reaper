#include "lib/base.hlsl"

#include "share/draw.hlsl"

VK_BINDING(0, 0) ConstantBuffer<DrawPassParams> pass_params;
VK_BINDING(1, 0) StructuredBuffer<DrawInstanceParams> instance_params;

struct VS_INPUT
{
    VK_LOCATION(0) float3 PositionMS;
    VK_LOCATION(1) float3 NormalMS;
    VK_LOCATION(2) float2 UV;
};

struct VS_OUTPUT
{
    float4 positionCS : SV_Position;
    float3 NormalVS;
    float2 UV;
};

VS_OUTPUT main(VS_INPUT input, uint instance_id : SV_InstanceID)
{
    const float3 positionMS = input.PositionMS;
    const float4 positionWS = float4(positionMS, 1.0) * instance_params[instance_id].model;
    const float4 positionCS = positionWS * pass_params.viewProj;

    VS_OUTPUT output;

    output.positionCS = positionCS;

    output.NormalVS = input.NormalMS * instance_params[instance_id].modelViewInvT;
    output.UV = input.UV;

    return output;
}
