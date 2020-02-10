#include "lib/base.hlsl"
#include "lib/indirect_command.hlsl"

#include "share/culling.hlsl"

//------------------------------------------------------------------------------
// Input

VK_CONSTANT(0) const bool spec_enable_backface_culling = true;
VK_CONSTANT(1) const bool spec_cull_cw = true;

VK_PUSH_CONSTANT() ConstantBuffer<CullPushConstants> consts;

VK_BINDING(0, 0) ByteAddressBuffer Indices;
VK_BINDING(1, 0) ByteAddressBuffer VertexPositions;
VK_BINDING(2, 0) StructuredBuffer<CullInstanceParams> instance_params;

//------------------------------------------------------------------------------
// Output

VK_BINDING(3, 0) RWByteAddressBuffer IndicesOut;
VK_BINDING(4, 0) RWByteAddressBuffer DrawCommandOut;
VK_BINDING(5, 0) globallycoherent RWByteAddressBuffer DrawCountOut;

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

    const uint vertex_size_in_bytes = 3 * 4;

    const float3 vpos0_ms = asfloat(VertexPositions.Load3(indices.x * vertex_size_in_bytes));
    const float3 vpos1_ms = asfloat(VertexPositions.Load3(indices.y * vertex_size_in_bytes));
    const float3 vpos2_ms = asfloat(VertexPositions.Load3(indices.z * vertex_size_in_bytes));

    const uint instance_id = gid.y;
    const float4x4 ms_to_cs_matrix = instance_params[instance_id].ms_to_cs_matrix;

    const float4 vpos0_cs = float4(vpos0_ms, 1.0) * ms_to_cs_matrix;
    const float4 vpos1_cs = float4(vpos1_ms, 1.0) * ms_to_cs_matrix;
    const float4 vpos2_cs = float4(vpos2_ms, 1.0) * ms_to_cs_matrix;

    const float3 vpos0_ndc = vpos0_cs.xyz / vpos0_cs.w;
    const float3 vpos1_ndc = vpos1_cs.xyz / vpos1_cs.w;
    const float3 vpos2_ndc = vpos2_cs.xyz / vpos2_cs.w;

    const float3 v0v1_ndc = vpos1_ndc - vpos0_ndc;
    const float3 v0v2_ndc = vpos2_ndc - vpos0_ndc;

    const bool is_front_face = spec_cull_cw ?
        cross(v0v1_ndc, v0v2_ndc).z <= 0.f :
        cross(v0v1_ndc, v0v2_ndc).z >= 0.f;

    const bool is_lane_enabled = dtid.x < consts.triangleCount;
    const bool is_visible = is_lane_enabled && (spec_enable_backface_culling ? is_front_face : true);

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
        DrawCountOut.InterlockedAdd(0, uint(1), draw_command_index); // FIXME Cast is needed for glslang

        IndirectDrawCommand command;
        command.indexCount = lds_triangle_count * 3;
        command.instanceCount = 1;
        command.firstIndex = lds_triangle_offset * 3;
        command.vertexOffset = 0;
        command.firstInstance = instance_id;

        store_draw_command(DrawCommandOut, draw_command_index * IndirectDrawCommandSize * 4, command);
    }
}
