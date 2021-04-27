#include "lib/base.hlsl"

#include "share/draw.hlsl"

VK_BINDING(0, 0) ConstantBuffer<DrawPassParams> pass_params;
VK_BINDING(1, 0) StructuredBuffer<DrawInstanceParams> instance_params;

struct VS_INPUT
{
    float3 PositionMS   : TEXCOORD0;
    float3 NormalMS     : TEXCOORD1;
    float2 UV           : TEXCOORD2;
};

struct VS_OUTPUT
{
    float4 PositionCS   : SV_Position;
    float3 PositionVS   : TEXCOORD0;
    float3 NormalVS     : TEXCOORD1;
    float2 UV           : TEXCOORD2;
    float3 PositionWS   : TEXCOORD3;
};

void main(in VS_INPUT input, uint instance_id : SV_InstanceID, out VS_OUTPUT output)
{
    const float3 positionMS = input.PositionMS;
    const float3 positionWS = mul(instance_params[instance_id].model, float4(positionMS, 1.0));
    const float3 positionVS = mul(pass_params.view, float4(positionWS, 1.0));
    const float4 positionCS = mul(pass_params.proj, float4(positionVS, 1.0));

    output.PositionCS = positionCS;
    output.PositionVS = positionVS;
    output.NormalVS = normalize(mul(instance_params[instance_id].normal_ms_to_vs_matrix, input.NormalMS));
    output.UV = input.UV;
    output.PositionWS = positionWS;
}
