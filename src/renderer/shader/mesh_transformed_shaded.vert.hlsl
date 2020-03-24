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
    const float4 positionWS = instance_params[instance_id].model * float4(positionMS, 1.0);
    const float4 positionCS = pass_params.viewProj * positionWS;

    VS_OUTPUT output;

    output.positionCS = positionCS;

    output.NormalVS = instance_params[instance_id].normal_ms_to_vs_matrix * input.NormalMS;

    output.UV = input.UV;

    return output;
}
