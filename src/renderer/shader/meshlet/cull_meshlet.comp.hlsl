#include "lib/base.hlsl"

#include "share/meshlet.hlsl"
#include "share/meshlet_culling.hlsl"

//------------------------------------------------------------------------------
// Input

VK_CONSTANT(0) const bool spec_enable_frustum_culling = true;

// https://github.com/KhronosGroup/glslang/issues/1629
#if defined(_DXC)
VK_PUSH_CONSTANT() CullMeshletPushConstants consts;
#else
VK_PUSH_CONSTANT() ConstantBuffer<CullMeshletPushConstants> consts;
#endif

VK_BINDING(0, 0) StructuredBuffer<Meshlet> meshlets;
VK_BINDING(1, 0) StructuredBuffer<CullMeshInstanceParams> cull_mesh_instance_params;

//------------------------------------------------------------------------------
// Output

VK_BINDING(2, 0) globallycoherent RWByteAddressBuffer Counters;
VK_BINDING(3, 0) RWStructuredBuffer<MeshletOffsets> meshlets_offsets_out;

//------------------------------------------------------------------------------

groupshared uint lds_meshlet_count;
groupshared uint lds_meshlet_offset;

[numthreads(MeshletCullThreadCount, 1, 1)]
void main(/*uint3 gtid : SV_GroupThreadID,*/
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    if (gi == 0)
    {
        lds_meshlet_count = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const Meshlet meshlet = meshlets[consts.meshlet_offset + dtid.x];

    const uint cull_instance_id = consts.cull_instance_offset + gid.y;
    const bool is_active = dtid.x < consts.meshlet_count;

    bool is_visible = is_active;

    if (spec_enable_frustum_culling)
    {
        const CullMeshInstanceParams mesh_instance = cull_mesh_instance_params[consts.cull_instance_offset];

        // FIXME Gigantic hack
        const float3 meshlet_center_ms = meshlet.center;
        const float4 meshlet_center_cs_h = mul(mesh_instance.ms_to_cs_matrix, float4(meshlet_center_ms, 1.0));
        const float3 meshlet_center_cs = meshlet_center_cs_h.xyz / meshlet_center_cs_h.w;

        const bool x_cull_test = meshlet_center_cs.x < -1.0 || meshlet_center_cs.x > 1.0;
        const bool y_cull_test = meshlet_center_cs.y < -1.0 || meshlet_center_cs.y > 1.0;
        const bool z_cull_test = meshlet_center_cs.z <  0.0 || meshlet_center_cs.z > 1.0;

        const bool frustum_test = !(x_cull_test || y_cull_test || z_cull_test);
        is_visible = is_visible && frustum_test;
    }

    const uint visible_count = WaveActiveCountBits(is_visible);
    const uint visible_prefix_count = WavePrefixCountBits(is_visible);

    uint wave_meshlet_offset;

    if (WaveIsFirstLane())
    {
        InterlockedAdd(lds_meshlet_count, visible_count, wave_meshlet_offset);
    }

    wave_meshlet_offset = WaveReadLaneFirst(wave_meshlet_offset);

    GroupMemoryBarrierWithGroupSync();

    if (gi == 0)
    {
        Counters.InterlockedAdd(MeshletCounterOffset * 4, lds_meshlet_count, lds_meshlet_offset);
    }

    GroupMemoryBarrierWithGroupSync();

    const uint output_meshlet_index = lds_meshlet_offset + wave_meshlet_offset + visible_prefix_count;

    if (is_visible)
    {
        // Bake in mesh offsets
        MeshletOffsets offsets;
        offsets.index_offset = meshlet.index_offset + consts.first_index;
        offsets.index_count = meshlet.index_count;
        offsets.vertex_offset = meshlet.vertex_offset + consts.first_vertex;
        offsets.cull_instance_id = cull_instance_id;

        meshlets_offsets_out[output_meshlet_index] = offsets;
    }
}
