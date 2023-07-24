#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "debug_geometry_private.share.hlsl"

#include "draw.hlsl"

VK_BINDING(0, 0) ByteAddressBuffer VertexPositionsMS;
VK_BINDING(1, 0) StructuredBuffer<DebugGeometryInstance> InstanceBuffer;

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
    uint api_instance_id : SV_InstanceID;
};

void main(VS_INPUT input, out VS_OUTPUT output)
{
    const DebugGeometryInstance instance = InstanceBuffer[input.api_instance_id];

    const float3 vpos_ms = pull_position(VertexPositionsMS, input.vertex_id);
    const float3 position_ms = vpos_ms * instance.radius;
    const float4 position_cs = mul(instance.ms_to_cs, float4(position_ms, 1.0));

    output.position = position_cs;
    output.color = instance.color;
}
