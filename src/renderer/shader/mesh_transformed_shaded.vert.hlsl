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
    nointerpolation uint texture_index : TEXCOORD4;
};

void main(in VS_INPUT input, uint instance_id : SV_InstanceID, out VS_OUTPUT output)
{
    const DrawInstanceParams instance_data = instance_params[instance_id];

    const float3 positionMS = input.PositionMS;
    const float3 positionWS = mul(instance_data.model, float4(positionMS, 1.0));
    const float3 positionVS = mul(pass_params.view, float4(positionWS, 1.0));
    const float4 positionCS = mul(pass_params.proj, float4(positionVS, 1.0));

    output.PositionCS = positionCS;
    output.PositionVS = positionVS;
    output.NormalVS = normalize(mul(instance_data.normal_ms_to_vs_matrix, input.NormalMS));
    output.UV = input.UV;
    output.PositionWS = positionWS;
    output.texture_index = instance_data.texture_index;
}
