#include "lib/base.hlsl"

#include "lib/brdf.hlsl"
#include "lib/barycentrics.hlsl"
#include "lib/vertex_pull.hlsl"
#include "gbuffer/gbuffer.hlsl"
#include "meshlet/meshlet.share.hlsl"
#include "forward.share.hlsl" // FIXME

#include "vis_buffer.hlsl"
#include "fill_gbuffer.share.hlsl"

VK_PUSH_CONSTANT_HELPER(FillGBufferPushConstants) push;

VK_BINDING(0, Slot_VisBuffer) Texture2D<VisBufferRawType> VisBuffer;
VK_BINDING(0, Slot_GBuffer0) RWTexture2D<GBuffer0Type> GBuffer0;
VK_BINDING(0, Slot_GBuffer1) RWTexture2D<GBuffer1Type> GBuffer1;
VK_BINDING(0, Slot_instance_params) StructuredBuffer<ForwardInstanceParams> instance_params;
VK_BINDING(0, Slot_visible_index_buffer) ByteAddressBuffer visible_index_buffer;
VK_BINDING(0, Slot_buffer_position_ms) ByteAddressBuffer buffer_position_ms;
VK_BINDING(0, Slot_buffer_normal_ms) ByteAddressBuffer buffer_normal_ms;
VK_BINDING(0, Slot_buffer_uv) ByteAddressBuffer buffer_uv;
VK_BINDING(0, Slot_visible_meshlets) StructuredBuffer<VisibleMeshlet> visible_meshlets;
VK_BINDING(0, Slot_diffuse_map_sampler) SamplerState diffuse_map_sampler;
VK_BINDING(0, Slot_diffuse_maps) Texture2D<float3> diffuse_maps[DiffuseMapMaxCount];

struct VertexData
{
    float3 position_ms;
    float3 normal_ms;
    float2 uv;
};

[numthreads(GBufferFillThreadCountX, GBufferFillThreadCountY, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    uint2 position_ts = dtid.xy;

    if (any(position_ts >= push.extent_ts))
    {
        return;
    }

    VisBufferRawType vis_buffer_raw = VisBuffer.Load(uint3(position_ts, 0));

    if (!is_vis_buffer_valid(vis_buffer_raw))
    {
        return;
    }

    float2 position_uv = ts_to_uv(position_ts, push.extent_ts_inv);
    float2 position_ndc = uv_to_ndc(position_uv);

    VisibilityBuffer vis_buffer = decode_vis_buffer(vis_buffer_raw);

    VisibleMeshlet visible_meshlet = visible_meshlets[vis_buffer.visible_meshlet_index];

    uint visible_index_offset = visible_meshlet.visible_index_offset + vis_buffer.meshlet_triangle_id * 3;
    uint3 indices = visible_index_buffer.Load3(visible_index_offset * MeshletIndexSizeBytes);
    indices += visible_meshlet.vertex_offset;

    VertexData p0;
    VertexData p1;
    VertexData p2;

    // FIXME AoS or SoA?
    p0.position_ms = pull_position(buffer_position_ms, indices.x);
    p1.position_ms = pull_position(buffer_position_ms, indices.y);
    p2.position_ms = pull_position(buffer_position_ms, indices.z);

    p0.normal_ms = pull_normal(buffer_normal_ms, indices.x);
    p1.normal_ms = pull_normal(buffer_normal_ms, indices.y);
    p2.normal_ms = pull_normal(buffer_normal_ms, indices.z);

    p0.uv = pull_uv(buffer_uv, indices.x);
    p1.uv = pull_uv(buffer_uv, indices.y);
    p2.uv = pull_uv(buffer_uv, indices.z);

    ForwardInstanceParams instance_data = instance_params[visible_meshlet.mesh_instance_id];
    float4 p0_cs = mul(instance_data.ms_to_cs_matrix, float4(p0.position_ms, 1.0));
    float4 p1_cs = mul(instance_data.ms_to_cs_matrix, float4(p1.position_ms, 1.0));
    float4 p2_cs = mul(instance_data.ms_to_cs_matrix, float4(p2.position_ms, 1.0));

    float3 cs_w_inv = rcp(float3(p0_cs.w, p1_cs.w, p2_cs.w));
    float2 p0_ndc = p0_cs.xy * cs_w_inv.x;
    float2 p1_ndc = p1_cs.xy * cs_w_inv.y;
    float2 p2_ndc = p2_cs.xy * cs_w_inv.z;

    BarycentricsWithDerivatives barycentrics = compute_barycentrics_with_derivatives(p0_ndc, p1_ndc, p2_ndc, cs_w_inv, position_ndc, push.extent_ts_inv);

    float3 uvx = interpolate_barycentrics_with_derivatives(barycentrics, p0.uv.x, p1.uv.x, p2.uv.x);
    float3 uvy = interpolate_barycentrics_with_derivatives(barycentrics, p0.uv.y, p1.uv.y, p2.uv.y);

    float2 uv = float2(uvx.x, uvy.x);
    float2 uv_ddx = float2(uvx.y, uvy.y);
    float2 uv_ddy = float2(uvx.z, uvy.z);

    StandardMaterial material;
#if defined(_DXC)
    material.albedo = diffuse_maps[NonUniformResourceIndex(instance_data.texture_index)].SampleGrad(diffuse_map_sampler, uv, uv_ddx, uv_ddy);
#else
    // FIXME This code is wrong but that's the best I can do right now.
    // https://github.com/KhronosGroup/glslang/issues/1637
    material.albedo = diffuse_maps[instance_data.texture_index].SampleGrad(diffuse_map_sampler, uv, uv_ddx, uv_ddy);
#endif
    material.roughness = 0.5;
    material.f0 = 0.1;

    float3 normal_ms = interpolate_barycentrics_simple_float3(barycentrics.lambda, p0.normal_ms, p1.normal_ms, p2.normal_ms);
    float3 normal_vs = mul(instance_data.normal_ms_to_vs_matrix, normal_ms);

    GBuffer gbuffer;
    gbuffer.albedo = material.albedo;
    gbuffer.roughness = material.roughness;
    gbuffer.f0 = material.f0;
    gbuffer.normal_vs = normalize(normal_vs);

    GBufferRaw gbuffer_raw = encode_gbuffer(gbuffer);
    GBuffer0[position_ts] = gbuffer_raw.rt0;
    GBuffer1[position_ts] = gbuffer_raw.rt1;
}
