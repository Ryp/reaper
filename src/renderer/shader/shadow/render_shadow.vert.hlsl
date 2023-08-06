#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "shadow_map_pass.share.hlsl"

VK_BINDING(0, 0) ConstantBuffer<ShadowMapPassParams> pass_params;
VK_BINDING(0, 1) StructuredBuffer<ShadowMapInstanceParams> instance_params;

VK_BINDING(0, 2) ByteAddressBuffer buffer_position_ms;

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
};

struct VS_OUTPUT
{
    float4 position_cs : SV_Position;
};

void main(in VS_INPUT input, uint instance_id : SV_InstanceID, out VS_OUTPUT output)
{
    const float3 position_ms = pull_position(buffer_position_ms, input.vertex_id);
    const float4 position_cs = mul(instance_params[instance_id].ms_to_cs_matrix, float4(position_ms, 1.0));

    output.position_cs = position_cs;
}
