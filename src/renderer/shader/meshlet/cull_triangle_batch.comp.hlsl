#include "lib/base.hlsl"
#include "lib/indirect_command.hlsl"
#include "lib/vertex_pull.hlsl"

#include "meshlet.share.hlsl"
#include "meshlet_culling.share.hlsl"

//------------------------------------------------------------------------------
// Input

VK_CONSTANT(0) const bool spec_cull_cw = true;
VK_CONSTANT(1) const bool spec_enable_backface_culling = true;
VK_CONSTANT(2) const bool spec_enable_frustum_culling = true;
VK_CONSTANT(3) const bool spec_enable_small_triangle_culling = true;

VK_PUSH_CONSTANT_HELPER(CullPushConstants) consts;

VK_BINDING(0, 0) StructuredBuffer<MeshletOffsets> meshlets;
VK_BINDING(1, 0) ByteAddressBuffer Indices;
VK_BINDING(2, 0) ByteAddressBuffer buffer_position_ms;
VK_BINDING(3, 0) StructuredBuffer<CullMeshInstanceParams> cull_mesh_instance_params;

//------------------------------------------------------------------------------
// Output

VK_BINDING(4, 0) RWByteAddressBuffer IndicesOut;
VK_BINDING(5, 0) RWStructuredBuffer<DrawIndexedIndirectCommand> DrawCommandOut;
VK_BINDING(6, 0) globallycoherent RWByteAddressBuffer Counters;

//------------------------------------------------------------------------------

groupshared uint lds_triangle_count;
groupshared uint lds_triangle_offset;

// FIXME non active threads read OOB
[numthreads(MeshletMaxTriangleCount, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          /*uint3 dtid : SV_DispatchThreadID,*/
          uint  gi   : SV_GroupIndex)
{
    if (gi == 0)
    {
        lds_triangle_count = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint meshlet_index = gid.x;
    const MeshletOffsets meshlet = meshlets[meshlet_index];

    const uint input_triangle_index_offset = meshlet.index_offset + gtid.x * 3;

    const uint3 indices = Indices.Load3(input_triangle_index_offset * 4);
    const uint3 indices_with_vertex_offset = indices + meshlet.vertex_offset;

    // NOTE: We will read out of bounds, this might be wasteful - or even illegal. OOB reads in DirectX11 are defined to return zero, what about Vulkan?
    const float3 vpos0_ms = pull_position(buffer_position_ms, indices_with_vertex_offset.x);
    const float3 vpos1_ms = pull_position(buffer_position_ms, indices_with_vertex_offset.y);
    const float3 vpos2_ms = pull_position(buffer_position_ms, indices_with_vertex_offset.z);

    const CullMeshInstanceParams mesh_instance = cull_mesh_instance_params[meshlet.cull_instance_id];

    const float4 vpos0_cs = mul(mesh_instance.ms_to_cs_matrix, float4(vpos0_ms, 1.0));
    const float4 vpos1_cs = mul(mesh_instance.ms_to_cs_matrix, float4(vpos1_ms, 1.0));
    const float4 vpos2_cs = mul(mesh_instance.ms_to_cs_matrix, float4(vpos2_ms, 1.0));

    const float3 vpos0_ndc = vpos0_cs.xyz / vpos0_cs.w;
    const float3 vpos1_ndc = vpos1_cs.xyz / vpos1_cs.w;
    const float3 vpos2_ndc = vpos2_cs.xyz / vpos2_cs.w;

    const bool is_active = gtid.x < (meshlet.index_count / 3);

    bool is_visible = is_active;

    if (spec_enable_backface_culling)
    {
        const float3 v0v1_ndc = vpos1_ndc - vpos0_ndc;
        const float3 v0v2_ndc = vpos2_ndc - vpos0_ndc;

        const bool is_front_face = spec_cull_cw ?
            cross(v0v1_ndc, v0v2_ndc).z <= 0.f :
            cross(v0v1_ndc, v0v2_ndc).z >= 0.f;

        is_visible = is_visible && is_front_face;
    }

    if (spec_enable_frustum_culling)
    {
        const float3 vpos012x_ndc = float3(vpos0_ndc.x, vpos1_ndc.x, vpos2_ndc.x);
        const float3 vpos012y_ndc = float3(vpos0_ndc.y, vpos1_ndc.y, vpos2_ndc.y);
        const float3 vpos012z_ndc = float3(vpos0_ndc.z, vpos1_ndc.z, vpos2_ndc.z);

        const bool x_cull_test = all(vpos012x_ndc < -1.0) || all(vpos012x_ndc > 1.0);
        const bool y_cull_test = all(vpos012y_ndc < -1.0) || all(vpos012y_ndc > 1.0);

        // TODO Bench this test, the ALU cost might not be worth it.
        const bool z_cull_test = all(vpos012z_ndc < 0.0)  || all(vpos012z_ndc > 1.0);

        const bool frustum_test = !(x_cull_test || y_cull_test || z_cull_test);
        is_visible = is_visible && frustum_test;
    }

    if (spec_enable_small_triangle_culling)
    {
        const float3 vpos012x_ndc = float3(vpos0_ndc.x, vpos1_ndc.x, vpos2_ndc.x);
        const float3 vpos012y_ndc = float3(vpos0_ndc.y, vpos1_ndc.y, vpos2_ndc.y);

        const float2 vpos012_ndc_min = float2(min3(vpos012x_ndc), min3(vpos012y_ndc));
        const float2 vpos012_ndc_max = float2(max3(vpos012x_ndc), max3(vpos012y_ndc));

        const float2 vpos012_ts_min = ndc_to_ts(vpos012_ndc_min, consts.output_size_ts);
        const float2 vpos012_ts_max = ndc_to_ts(vpos012_ndc_max, consts.output_size_ts);

        const bool small_triangle_test = !any(round(vpos012_ts_min) == round(vpos012_ts_max));

        is_visible = is_visible && small_triangle_test;
    }

    const uint visible_count = WaveActiveCountBits(is_visible);
    const uint visible_prefix_count = WavePrefixCountBits(is_visible);

    uint wave_triangle_offset;

    if (WaveIsFirstLane())
    {
        InterlockedAdd(lds_triangle_count, visible_count, wave_triangle_offset);
    }

    wave_triangle_offset = WaveReadLaneFirst(wave_triangle_offset);

    GroupMemoryBarrierWithGroupSync();

    if (gi == 0)
    {
        Counters.InterlockedAdd(TriangleCounterOffset * 4, lds_triangle_count, lds_triangle_offset);
    }

    GroupMemoryBarrierWithGroupSync();

    const uint output_triangle_index = lds_triangle_offset + wave_triangle_offset + visible_prefix_count;

    if (is_visible)
    {
        IndicesOut.Store3(output_triangle_index * 3 * 4, indices);
    }

    if (gi == 0 && lds_triangle_count > 0)
    {
        uint draw_command_index;

        Counters.InterlockedAdd(DrawCommandCounterOffset * 4, 1u, draw_command_index);

        DrawIndexedIndirectCommand command;
        command.indexCount = lds_triangle_count * 3;
        command.instanceCount = 1;
        command.firstIndex = lds_triangle_offset * 3;
        command.vertexOffset = meshlet.vertex_offset;
        command.firstInstance = mesh_instance.instance_id;

        DrawCommandOut[draw_command_index] = command;
    }
}
