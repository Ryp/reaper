#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "forward.share.hlsl"
#include "meshlet/meshlet.share.hlsl"

VK_BINDING(0, 0) StructuredBuffer<ForwardInstanceParams> instance_params;
VK_BINDING(1, 0) StructuredBuffer<VisibleMeshlet> visible_meshlets;
VK_BINDING(2, 0) ByteAddressBuffer buffer_position_ms;

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
    uint visible_meshlet_index : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 position_cs : SV_Position;
    nointerpolation uint visible_meshlet_index : TEXCOORD0;
};

void main(in VS_INPUT input, out VS_OUTPUT output)
{
    VisibleMeshlet visible_meshlet = visible_meshlets[input.visible_meshlet_index];

    float3 position_ms = pull_position(buffer_position_ms, input.vertex_id + visible_meshlet.vertex_offset);

    ForwardInstanceParams instance_data = instance_params[visible_meshlet.mesh_instance_id];

    output.position_cs = mul(instance_data.ms_to_cs_matrix, float4(position_ms, 1.0));
    output.visible_meshlet_index = input.visible_meshlet_index;
}
