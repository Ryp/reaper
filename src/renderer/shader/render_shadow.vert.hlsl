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

void main(in VS_INPUT input, uint instance_id : SV_InstanceID, out VS_OUTPUT output)
{
    const float3 positionMS = input.PositionMS;
    const float4 positionCS = mul(instance_params[instance_id].ms_to_cs_matrix, float4(positionMS, 1.0));

    output.PositionCS = positionCS;
}
