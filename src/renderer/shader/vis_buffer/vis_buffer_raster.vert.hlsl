#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "forward.share.hlsl"

VK_BINDING(0, 0) StructuredBuffer<ForwardInstanceParams> instance_params;
VK_BINDING(1, 0) ByteAddressBuffer buffer_position_ms;

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
    uint instance_id : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 position_cs : SV_Position;
    nointerpolation uint triangle_id : TEXCOORD0;
    nointerpolation uint instance_id : TEXCOORD1;
};

void main(in VS_INPUT input, out VS_OUTPUT output)
{
    const float3 position_ms = pull_position(buffer_position_ms, input.vertex_id);

    const ForwardInstanceParams instance_data = instance_params[input.instance_id];

    output.position_cs = mul(instance_data.ms_to_cs_matrix, float4(position_ms, 1.0));
    output.triangle_id = input.vertex_id / 3; // FIXME assumes indexing is actually flat
    output.instance_id = input.instance_id;
}
