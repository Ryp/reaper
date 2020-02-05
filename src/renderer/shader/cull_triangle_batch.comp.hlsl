#include "lib/base.hlsl"

struct CullInstanceParams
{
    float4x4 ms_to_cs_matrix;
};

struct PushConstants
{
    uint commandIndex;
    uint triangleCount;
    uint firstIndex;
};

static const uint ComputeCullingBatchSize = 256;

//------------------------------------------------------------------------------
// Input

VK_PUSH_CONSTANT() ConstantBuffer<PushConstants> consts;

VK_BINDING(0, 0) ByteAddressBuffer Indices;
VK_BINDING(1, 0) ByteAddressBuffer VertexPositions;
VK_BINDING(2, 0) StructuredBuffer<CullInstanceParams> instance_params;

//------------------------------------------------------------------------------
// Output

VK_BINDING(3, 0) RWByteAddressBuffer IndicesOut;
VK_BINDING(4, 0) RWByteAddressBuffer DrawCommandOut;
VK_BINDING(5, 0) globallycoherent RWByteAddressBuffer DrawCountOut;

//------------------------------------------------------------------------------

[numthreads(ComputeCullingBatchSize, 1, 1)]
void main(/*uint3 gtid : SV_GroupThreadID,*/
          /*uint3 gid  : SV_GroupID,*/
          uint3 dtid : SV_DispatchThreadID,
          /*uint  gi   : SV_GroupIndex*/)
{
    if (dtid.x == 0)
    {
        // FIXME Clear each frame
        if (consts.commandIndex == 0)
        {
            DrawCountOut.Store(0, 0);
            DrawCountOut.Store(4, 0);
        }

        DrawCountOut.Store(8, 0xFFFFFFFF);
        DrawCountOut.Store(12, 0);
    }

    AllMemoryBarrierWithGroupSync(); // FIXME
    //GroupMemoryBarrierWithGroupSync();

    const uint input_triangle_index_offset = consts.firstIndex + dtid.x * 3;

    const uint index0 = Indices.Load((input_triangle_index_offset + 0) * 4);
    const uint index1 = Indices.Load((input_triangle_index_offset + 1) * 4);
    const uint index2 = Indices.Load((input_triangle_index_offset + 2) * 4);

    const uint vertex_size_in_bytes = 3 * 4;

    // FIXME Load4 can load outside of allocated address
    const float3 vpos0_ms = asfloat(VertexPositions.Load4(index0 * vertex_size_in_bytes).xyz);
    const float3 vpos1_ms = asfloat(VertexPositions.Load4(index1 * vertex_size_in_bytes).xyz);
    const float3 vpos2_ms = asfloat(VertexPositions.Load4(index2 * vertex_size_in_bytes).xyz);

    const float4x4 ms_to_cs_matrix = instance_params[consts.commandIndex].ms_to_cs_matrix;

    const float4 vpos0_cs = float4(vpos0_ms, 1.0) * ms_to_cs_matrix;
    const float4 vpos1_cs = float4(vpos1_ms, 1.0) * ms_to_cs_matrix;
    const float4 vpos2_cs = float4(vpos2_ms, 1.0) * ms_to_cs_matrix;

    const float3 v0v1_cs = vpos1_cs.xyz - vpos0_cs.xyz;
    const float3 v0v2_cs = vpos2_cs.xyz - vpos0_cs.xyz;

    const bool is_ccw = cross(v0v1_cs, v0v2_cs).z <= 0.f;
    const bool is_enabled = dtid.x < consts.triangleCount;

    const bool is_visible = is_ccw && is_enabled;
    //const bool is_visible = is_enabled;

    const uint visible_count = WaveActiveCountBits(is_visible);
    const uint visible_prefix_count = WavePrefixCountBits(is_visible);

    // Synchronize waves
    uint wave_triangle_offset;

    if (WaveIsFirstLane())
    {
        DrawCountOut.InterlockedAdd(4, visible_count, wave_triangle_offset);

        DrawCountOut.InterlockedMin(8, wave_triangle_offset);

        DrawCountOut.InterlockedAdd(12, visible_count);
    }

    wave_triangle_offset = WaveReadLaneAt(wave_triangle_offset, 0);

    const uint output_triangle_index = wave_triangle_offset + visible_prefix_count - 1;

    if (is_visible)
    {
        IndicesOut.Store((output_triangle_index * 3 + 0) * 4, index0);
        IndicesOut.Store((output_triangle_index * 3 + 1) * 4, index1);
        IndicesOut.Store((output_triangle_index * 3 + 2) * 4, index2);
    }

    AllMemoryBarrierWithGroupSync();

    if (dtid.x == 0)
    {
        uint draw_command_index;
        DrawCountOut.InterlockedAdd(0, uint(1), draw_command_index); // FIXME Cast is needed for glslang

        uint min_triangle_offset = DrawCountOut.Load(8);
        uint triangle_count = DrawCountOut.Load(12);

        const uint draw_command_size = 5;
        DrawCommandOut.Store((draw_command_index * draw_command_size + 0) * 4, triangle_count * 3);
        DrawCommandOut.Store((draw_command_index * draw_command_size + 1) * 4, 1);
        DrawCommandOut.Store((draw_command_index * draw_command_size + 2) * 4, min_triangle_offset * 3);
        DrawCommandOut.Store((draw_command_index * draw_command_size + 3) * 4, 0);
        DrawCommandOut.Store((draw_command_index * draw_command_size + 4) * 4, consts.commandIndex);
    }
}
