#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "forward.share.hlsl"
#include "meshlet/meshlet.share.hlsl"

VK_BINDING(0, 0) ConstantBuffer<ForwardPassParams> pass_params;
VK_BINDING(1, 0) StructuredBuffer<ForwardInstanceParams> instance_params;
VK_BINDING(2, 0) StructuredBuffer<VisibleMeshlet> visible_meshlets;
VK_BINDING(3, 0) ByteAddressBuffer buffer_position_ms;
VK_BINDING(4, 0) ByteAddressBuffer buffer_normal_ms;
VK_BINDING(5, 0) ByteAddressBuffer buffer_uv;

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
    uint visible_meshlet_index : SV_InstanceID;
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
    VisibleMeshlet visible_meshlet = visible_meshlets[input.visible_meshlet_index];

    uint vertex_id = input.vertex_id + visible_meshlet.vertex_offset;

    const float3 position_ms = pull_position(buffer_position_ms, vertex_id);
    const float3 normal_ms = pull_normal(buffer_normal_ms, vertex_id);
    const float2 uv = pull_uv(buffer_uv, vertex_id);

    const ForwardInstanceParams instance_data = instance_params[visible_meshlet.mesh_instance_id];

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
