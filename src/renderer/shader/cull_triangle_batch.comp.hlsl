[[vk::binding(0, 0)]]
ByteAddressBuffer Indices;

[[vk::binding(1, 0)]]
ByteAddressBuffer VertexPositions;

struct CullInstanceParams
{
    float4x4 ms_to_cs_matrix;
};

[[vk::binding(2, 0)]]
StructuredBuffer<CullInstanceParams> instance_params;

[[vk::binding(3, 0)]]
RWByteAddressBuffer IndicesOut;

[[vk::binding(4, 0)]]
RWByteAddressBuffer DrawCommandOut;

[[vk::binding(5, 0)]]
RWByteAddressBuffer DrawCountOut;

groupshared uint lds_total_triangle_count;

static const uint3 thread_count = uint3(1024, 1, 1);

[numthreads(thread_count.x, thread_count.y, thread_count.z)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    if (gi == 0)
        lds_total_triangle_count = 0;

    // FIXME can we remove it?
    GroupMemoryBarrierWithGroupSync();

    const uint input_triangle_index = dtid.x * 3;

    const uint index0 = Indices.Load((input_triangle_index + 0) * 4);
    const uint index1 = Indices.Load((input_triangle_index + 1) * 4);
    const uint index2 = Indices.Load((input_triangle_index + 2) * 4);

    const uint vertex_size_in_bytes = 3 * 4;

    // FIXME Load4 can load outside of allocated address
    const float3 vpos0_ms = asfloat(VertexPositions.Load4(index0 * vertex_size_in_bytes).xyz);
    const float3 vpos1_ms = asfloat(VertexPositions.Load4(index1 * vertex_size_in_bytes).xyz);
    const float3 vpos2_ms = asfloat(VertexPositions.Load4(index2 * vertex_size_in_bytes).xyz);

    // FIXME get correct instance matrix
    const float4x4 ms_to_cs_matrix = instance_params[0].ms_to_cs_matrix;

    const float3 vpos0_cs = mul(float4(vpos0_ms, 1.0), ms_to_cs_matrix).xyz;
    const float3 vpos1_cs = mul(float4(vpos1_ms, 1.0), ms_to_cs_matrix).xyz;
    const float3 vpos2_cs = mul(float4(vpos2_ms, 1.0), ms_to_cs_matrix).xyz;

    const float3 v0v1_cs = vpos1_cs - vpos0_cs;
    const float3 v0v2_cs = vpos2_cs - vpos0_cs;

    const bool is_visible = cross(v0v1_cs, v0v2_cs).z >= 0.f;

    const uint visible_count = WaveActiveCountBits(is_visible);
    const uint visible_prefix_count = WavePrefixCountBits(is_visible);

    const uint lane_cout = WaveGetLaneCount();

    // Synchronize waves
    uint global_output_index;

    if (gi % lane_cout == 0)
    {
        InterlockedAdd(lds_total_triangle_count, visible_count, global_output_index);
    }

    global_output_index = WaveReadLaneAt(global_output_index, 0);

    const uint output_triangle_index = global_output_index + visible_prefix_count - 1;

    if (is_visible)
    {
        IndicesOut.Store((output_triangle_index * 3 + 0) * 4, index0);
        IndicesOut.Store((output_triangle_index * 3 + 1) * 4, index1);
        IndicesOut.Store((output_triangle_index * 3 + 2) * 4, index2);
    }

    GroupMemoryBarrierWithGroupSync();

    if (gi == 0)
    {
        const uint index_count_offset = 0;
        DrawCommandOut.Store((index_count_offset + 0) * 4, lds_total_triangle_count * 3);
        DrawCommandOut.Store((index_count_offset + 5) * 4, lds_total_triangle_count * 3);
        DrawCommandOut.Store((index_count_offset + 10) * 4, lds_total_triangle_count * 3);

        const uint dummy_draw_count = 2; // FIXME set to 2 to demonstrate the feature
        DrawCountOut.Store(0, dummy_draw_count);
    }
}
