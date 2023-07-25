#include "lib/base.hlsl"

#include "lib/brdf.hlsl"
#include "lib/barycentrics.hlsl"
#include "lib/vertex_pull.hlsl"
#include "gbuffer/gbuffer.hlsl"

#include "vis_buffer.hlsl"
#include "fill_gbuffer.share.hlsl"

VK_PUSH_CONSTANT_HELPER(FillGBufferPushConstants) push;

VK_BINDING(0, 0) Texture2D<VisBufferRawType> VisBuffer;
VK_BINDING(1, 0) RWTexture2D<GBuffer0Type> GBuffer0;
VK_BINDING(2, 0) RWTexture2D<GBuffer1Type> GBuffer1;
#include "forward.share.hlsl" // FIXME
VK_BINDING(3, 0) StructuredBuffer<ForwardInstanceParams> instance_params;
VK_BINDING(4, 0) ByteAddressBuffer IndexBuffer; // FIXME
VK_BINDING(5, 0) ByteAddressBuffer buffer_position_ms;
VK_BINDING(6, 0) ByteAddressBuffer buffer_normal_ms;
VK_BINDING(7, 0) ByteAddressBuffer buffer_uv;
VK_BINDING(8, 0) SamplerState diffuse_map_sampler;
VK_BINDING(9, 0) Texture2D<float3> t_diffuse_map[];

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

    uint v0_id = vis_buffer.triangle_id * 3;
    uint v1_id = vis_buffer.triangle_id * 3 + 1;
    uint v2_id = vis_buffer.triangle_id * 3 + 2;

    VertexData p0;
    VertexData p1;
    VertexData p2;

    // FIXME AoS or SoA?
    p0.position_ms = pull_position(buffer_position_ms, v0_id);
    p1.position_ms = pull_position(buffer_position_ms, v1_id);
    p2.position_ms = pull_position(buffer_position_ms, v2_id);

    p0.normal_ms = pull_normal(buffer_normal_ms, v0_id);
    p1.normal_ms = pull_normal(buffer_normal_ms, v1_id);
    p2.normal_ms = pull_normal(buffer_normal_ms, v2_id);

    p0.uv = pull_uv(buffer_uv, v0_id);
    p1.uv = pull_uv(buffer_uv, v1_id);
    p2.uv = pull_uv(buffer_uv, v2_id);

    ForwardInstanceParams instance_data = instance_params[vis_buffer.instance_id];
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
    material.albedo = t_diffuse_map[NonUniformResourceIndex(instance_data.texture_index)].SampleGrad(diffuse_map_sampler, uv, uv_ddx, uv_ddy);
#else
    // FIXME This code is wrong but that's the best I can do right now.
    // https://github.com/KhronosGroup/glslang/issues/1637
    material.albedo = t_diffuse_map[instance_data.texture_index].SampleGrad(diffuse_map_sampler, uv, uv_ddx, uv_ddy);
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
