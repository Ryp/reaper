#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "forward.share.hlsl"
#include "mesh_instance.share.hlsl"
#include "meshlet/meshlet.share.hlsl"

VK_BINDING(0, 0) ConstantBuffer<ForwardPassParams> pass_params;
VK_BINDING(0, 1) StructuredBuffer<MeshInstance> instance_params;

VK_BINDING(0, 3) StructuredBuffer<VisibleMeshlet> visible_meshlets;
VK_BINDING(0, 4) ByteAddressBuffer buffer_position_ms;
VK_BINDING(0, 5) ByteAddressBuffer buffer_attributes;

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
    float3 tangent_vs   : TEXCOORD2;
    nointerpolation float bitangent_sign : TEXCOORD3;
    float2 UV           : TEXCOORD4;
    float3 PositionWS   : TEXCOORD5;
    nointerpolation uint material_index : TEXCOORD6;
};

void main(in VS_INPUT input, out VS_OUTPUT output)
{
    // NOTE: If we cared enough to split meshlet culling for forward passes,
    // we could remove the need for this buffer (brought by visibility rendering)
    // and directly write the indirect draw commands in the right format.
    VisibleMeshlet visible_meshlet = visible_meshlets[input.visible_meshlet_index];

    uint vertex_id = input.vertex_id + visible_meshlet.vertex_offset;

    const float3 position_ms = pull_position(buffer_position_ms, vertex_id);
    const float3 normal_ms = pull_normal(buffer_attributes, vertex_id);
    const float4 tangent_ms = pull_tangent(buffer_attributes, vertex_id);
    const float2 uv = pull_uv(buffer_attributes, vertex_id);

    const MeshInstance instance_data = instance_params[visible_meshlet.mesh_instance_id];

    const float3 position_ws = mul(instance_data.ms_to_ws_matrix, float4(position_ms, 1.0));
    const float3 position_vs = mul(pass_params.ws_to_vs_matrix, float4(position_ws, 1.0));
    const float4 position_cs = mul(pass_params.ws_to_cs_matrix, float4(position_ws, 1.0));

    output.PositionCS = position_cs;
    output.PositionVS = position_vs;
    output.NormalVS = normalize(mul(instance_data.normal_ms_to_vs_matrix, normal_ms));
    output.tangent_vs = normalize(mul(instance_data.normal_ms_to_vs_matrix, tangent_ms.xyz));
    output.bitangent_sign = tangent_ms.w;
    output.UV = uv;
    output.PositionWS = position_ws;
    output.material_index = instance_data.material_index;
}
