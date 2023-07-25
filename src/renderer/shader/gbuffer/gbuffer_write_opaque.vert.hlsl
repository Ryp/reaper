#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "forward.share.hlsl"

VK_BINDING(0, 0) StructuredBuffer<ForwardInstanceParams> instance_params;
VK_BINDING(1, 0) ByteAddressBuffer buffer_position_ms;
VK_BINDING(2, 0) ByteAddressBuffer buffer_normal_ms;
VK_BINDING(3, 0) ByteAddressBuffer buffer_uv;

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
    uint instance_id : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 PositionCS   : SV_Position;
    float3 NormalVS     : TEXCOORD0;
    float2 UV           : TEXCOORD1;
    nointerpolation uint texture_index : TEXCOORD2;
};

void main(in VS_INPUT input, out VS_OUTPUT output)
{
    const float3 position_ms = pull_position(buffer_position_ms, input.vertex_id);
    const float3 normal_ms = pull_normal(buffer_normal_ms, input.vertex_id);
    const float2 uv = pull_uv(buffer_uv, input.vertex_id);

    const ForwardInstanceParams instance_data = instance_params[input.instance_id];

    output.PositionCS = mul(instance_data.ms_to_cs_matrix, float4(position_ms, 1.0));
    output.NormalVS = normalize(mul(instance_data.normal_ms_to_vs_matrix, normal_ms));
    output.UV = uv;
    output.texture_index = instance_data.texture_index;
}
