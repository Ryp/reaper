#include "lib/base.hlsl"

#include "share/draw.hlsl"

VK_BINDING(0, 0) ConstantBuffer<DrawPassParams> pass_params;
VK_BINDING(1, 0) StructuredBuffer<DrawInstanceParams> instance_params;

struct VS_INPUT
{
    VK_LOCATION(0) float3 PositionMS : TEXCOORD0;
    VK_LOCATION(1) float3 NormalMS  : TEXCOORD1;
    VK_LOCATION(2) float2 UV : TEXCOORD2;
};

struct VS_OUTPUT
{
    float4 positionCS : SV_Position;
    float3 NormalVS : TEXCOORD0;
    float2 UV : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input, uint instance_id : SV_InstanceID)
{
    const float3 positionMS = input.PositionMS;
    const float3 positionWS = mul(instance_params[instance_id].model, float4(positionMS, 1.0));

    // FIXME Clearly we have a problem of codegen here.
#ifdef _DXC
    const float4 positionCS = mul(pass_params.viewProj, float4(positionWS.xyz, 1.0));
#else
    const float4 positionCS = mul(float4(positionWS.xyz, 1.0), pass_params.viewProj);
#endif

    VS_OUTPUT output;

    output.positionCS = positionCS;

    output.NormalVS = mul(instance_params[instance_id].normal_ms_to_vs_matrix, input.NormalMS);

    output.UV = input.UV;

    return output;
}
