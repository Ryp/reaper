#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "meshlet/meshlet.share.hlsl"
#include "mesh_instance.share.hlsl"
#include "gbuffer_write_opaque.share.hlsl"

VK_BINDING(0, Slot_instance_params) StructuredBuffer<MeshInstance> instance_params;
VK_BINDING(0, Slot_visible_meshlets) StructuredBuffer<VisibleMeshlet> visible_meshlets;
VK_BINDING(0, Slot_buffer_position_ms) ByteAddressBuffer buffer_position_ms;
VK_BINDING(0, Slot_buffer_attributes) ByteAddressBuffer buffer_attributes;

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
    uint visible_meshlet_index : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 PositionCS   : SV_Position;
    float3 NormalVS     : TEXCOORD0;
    float3 tangent_vs   : TEXCOORD1;
    nointerpolation float bitangent_sign : TEXCOORD2;
    float2 UV           : TEXCOORD3;
    nointerpolation uint albedo_texture_index : TEXCOORD4;
    nointerpolation uint roughness_texture_index : TEXCOORD5;
    nointerpolation uint normal_texture_index : TEXCOORD6;
    nointerpolation uint ao_texture_index : TEXCOORD7;
};

void main(in VS_INPUT input, out VS_OUTPUT output)
{
    VisibleMeshlet visible_meshlet = visible_meshlets[input.visible_meshlet_index];

    uint vertex_id = input.vertex_id + visible_meshlet.vertex_offset;

    const float3 position_ms = pull_position(buffer_position_ms, vertex_id);
    const float3 normal_ms = pull_normal(buffer_attributes, vertex_id);
    const float4 tangent_ms = pull_tangent(buffer_attributes, vertex_id);
    const float2 uv = pull_uv(buffer_attributes, vertex_id);

    const MeshInstance instance_data = instance_params[visible_meshlet.mesh_instance_id];

    output.PositionCS = mul(instance_data.ms_to_cs_matrix, float4(position_ms, 1.0));
    output.NormalVS = normalize(mul(instance_data.normal_ms_to_vs_matrix, normal_ms));
    output.tangent_vs = normalize(mul(instance_data.normal_ms_to_vs_matrix, tangent_ms.xyz));
    output.bitangent_sign = tangent_ms.w;
    output.UV = uv;
    output.albedo_texture_index = instance_data.albedo_texture_index;
    output.roughness_texture_index = instance_data.roughness_texture_index;
    output.ao_texture_index = instance_data.ao_texture_index;
}
