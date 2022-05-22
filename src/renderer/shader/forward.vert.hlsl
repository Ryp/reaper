#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "share/forward.hlsl"

VK_BINDING(0, 0) ConstantBuffer<ForwardPassParams> pass_params;
VK_BINDING(1, 0) StructuredBuffer<ForwardInstanceParams> instance_params;

VK_BINDING(2, 0) ByteAddressBuffer buffer_position_ms;
VK_BINDING(3, 0) ByteAddressBuffer buffer_normal_ms;
VK_BINDING(4, 0) ByteAddressBuffer buffer_uv;

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
    uint instance_id : SV_InstanceID;
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

void main(in VS_INPUT input, out VS_OUTPUT output)
{
    const float3 position_ms = pull_position_ms(buffer_position_ms, input.vertex_id);
    const float3 normal_ms = pull_normal_ms(buffer_normal_ms, input.vertex_id);
    const float2 uv = pull_uv(buffer_uv, input.vertex_id);

    const ForwardInstanceParams instance_data = instance_params[input.instance_id];

    const float3 position_ws = mul(instance_data.ms_to_ws_matrix, float4(position_ms, 1.0));
    const float3 position_vs = mul(pass_params.ws_to_vs_matrix, float4(position_ws, 1.0));
    const float4 position_cs = mul(pass_params.vs_to_cs_matrix, float4(position_vs, 1.0));

    output.PositionCS = position_cs;
    output.PositionVS = position_vs;
    output.NormalVS = normalize(mul(instance_data.normal_ms_to_vs_matrix, normal_ms));
    output.UV = uv;
    output.PositionWS = position_ws;
    output.texture_index = instance_data.texture_index;
}
