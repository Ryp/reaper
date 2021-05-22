#include "lib/base.hlsl"
#include "lib/indirect_command.hlsl"

#include "share/culling.hlsl"

//------------------------------------------------------------------------------
// Input

VK_CONSTANT(0) const bool spec_cull_cw = true;
VK_CONSTANT(1) const bool spec_enable_backface_culling = true;
VK_CONSTANT(2) const bool spec_enable_frustum_culling = true;
VK_CONSTANT(3) const bool spec_enable_small_triangle_culling = true;

// https://github.com/KhronosGroup/glslang/issues/1629
#if defined(_DXC)
VK_PUSH_CONSTANT() CullPushConstants consts;
#else
VK_PUSH_CONSTANT() ConstantBuffer<CullPushConstants> consts;
#endif

VK_BINDING(0, 0) ConstantBuffer<CullPassParams> pass_params;

VK_BINDING(1, 0) ByteAddressBuffer Indices;
VK_BINDING(2, 0) ByteAddressBuffer VertexPositions;
VK_BINDING(3, 0) StructuredBuffer<CullMeshInstanceParams> cull_mesh_instance_params;

//------------------------------------------------------------------------------
// Output

VK_BINDING(4, 0) RWByteAddressBuffer IndicesOut;
VK_BINDING(5, 0) RWByteAddressBuffer DrawCommandOut;
VK_BINDING(6, 0) globallycoherent RWByteAddressBuffer DrawCountOut;

//------------------------------------------------------------------------------

groupshared uint lds_triangle_count;
groupshared uint lds_triangle_offset;

[numthreads(ComputeCullingGroupSize, 1, 1)]
void main(/*uint3 gtid : SV_GroupThreadID,*/
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    if (gi == 0)
    {
        lds_triangle_count = 0;
        lds_triangle_offset = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint input_triangle_index_offset = consts.firstIndex + dtid.x * 3;

    const uint3 indices = Indices.Load3(input_triangle_index_offset * 4);
    const uint vertex_size_in_bytes = 3 * 4; // FIXME hardcode position vertex size
    const uint3 indices_with_vertex_offset_bytes = (indices + consts.firstVertex.xxx) * vertex_size_in_bytes;

    // NOTE: We will read out of bounds, this might be wasteful - or even illegal. OOB reads in DirectX11 are defined to return zero, what about Vulkan?
    const float3 vpos0_ms = asfloat(VertexPositions.Load3(indices_with_vertex_offset_bytes.x));
    const float3 vpos1_ms = asfloat(VertexPositions.Load3(indices_with_vertex_offset_bytes.y));
    const float3 vpos2_ms = asfloat(VertexPositions.Load3(indices_with_vertex_offset_bytes.z));

    const uint cull_instance_id = consts.firstCullInstance + gid.y;
    const float4x4 ms_to_cs_matrix = cull_mesh_instance_params[cull_instance_id].ms_to_cs_matrix;
    const uint instance_id = cull_mesh_instance_params[cull_instance_id].instance_id;

    const float4 vpos0_cs = mul(ms_to_cs_matrix, float4(vpos0_ms, 1.0));
    const float4 vpos1_cs = mul(ms_to_cs_matrix, float4(vpos1_ms, 1.0));
    const float4 vpos2_cs = mul(ms_to_cs_matrix, float4(vpos2_ms, 1.0));

    const float3 vpos0_ndc = vpos0_cs.xyz / vpos0_cs.w;
    const float3 vpos1_ndc = vpos1_cs.xyz / vpos1_cs.w;
    const float3 vpos2_ndc = vpos2_cs.xyz / vpos2_cs.w;

    const bool is_lane_enabled = dtid.x < consts.triangleCount;
    bool is_visible = is_lane_enabled;

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

        const float2 vpos012_ts_min = ndc_to_ts(vpos012_ndc_min, pass_params.output_size_ts);
        const float2 vpos012_ts_max = ndc_to_ts(vpos012_ndc_max, pass_params.output_size_ts);

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
        DrawCountOut.InterlockedAdd(4, lds_triangle_count, lds_triangle_offset);
    }

    GroupMemoryBarrierWithGroupSync();

    const uint output_triangle_index = lds_triangle_offset + wave_triangle_offset + visible_prefix_count - 1;

    if (is_visible)
    {
        IndicesOut.Store3(output_triangle_index * 3 * 4, indices);
    }

    if (gi == 0)
    {
        uint draw_command_index;

        // NOTE: casting to uint is needed for glslang
        // https://github.com/KhronosGroup/glslang/issues/2066
        DrawCountOut.InterlockedAdd(0, uint(1), draw_command_index);

        IndirectDrawCommand command;
        command.indexCount = lds_triangle_count * 3;
        command.instanceCount = 1;
        command.firstIndex = consts.outputIndexOffset + lds_triangle_offset * 3;
        command.vertexOffset = consts.firstVertex;
        command.firstInstance = instance_id;

        store_draw_command(DrawCommandOut, draw_command_index * IndirectDrawCommandSize * 4, command);
    }
}
