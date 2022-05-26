#include "lib/base.hlsl"
#include "lib/aabb.hlsl"
#include "share/meshlet.hlsl"
#include "share/meshlet_culling.hlsl"

//------------------------------------------------------------------------------
// Input

VK_CONSTANT(0) const bool spec_enable_frustum_culling = true;
VK_CONSTANT(1) const bool spec_enable_cone_culling = true;

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

        float3 meshlet_aabb_min_ndc = FLT_MAX;
        float3 meshlet_aabb_max_ndc = -FLT_MAX;

        for (uint i = 0; i < 8; i++)
        {
            const uint3 unit_offset = uint3(i, i >> 1, i >> 2) & 1;
            const float3 meshlet_vertex_ms = meshlet.center_ms + float3(unit_offset) * 2.0 * meshlet.radius - meshlet.radius;
            const float4 meshlet_vertex_cs = mul(mesh_instance.ms_to_cs_matrix, float4(meshlet_vertex_ms, 1.0));
            const float3 meshlet_vertex_ndc = meshlet_vertex_cs.xyz / meshlet_vertex_cs.w;

            meshlet_aabb_min_ndc = min(meshlet_aabb_min_ndc, meshlet_vertex_ndc);
            meshlet_aabb_max_ndc = max(meshlet_aabb_max_ndc, meshlet_vertex_ndc);
        }

        const AABB meshlet_aabb_ndc = build_aabb(meshlet_aabb_min_ndc, meshlet_aabb_max_ndc);

        const float3 frustum_aabb_min_ndc = float3(-1.0, -1.0, 0);
        const float3 frustum_aabb_max_ndc = float3(1.0, 1.0, 1.0);
        const AABB frustum_aabb_ndc = build_aabb(frustum_aabb_min_ndc, frustum_aabb_max_ndc);

        is_visible = is_visible && aabb_vs_aabb_test(frustum_aabb_ndc, meshlet_aabb_ndc);
    }

    // For backface culling with orthographic projection, use the following formula to reject backfacing clusters:
    // dot(view, cone_axis) >= cone_cutoff
    //
    // For perspective projection, you can the formula that needs cone apex in addition to axis & cutoff:
    //   dot(normalize(cone_apex - camera_position), cone_axis) >= cone_cutoff
    //
    // Alternatively, you can use the formula that doesn't need cone apex and uses bounding sphere instead:
    //   dot(normalize(center - camera_position), cone_axis) >= cone_cutoff + radius / length(center - camera_position)
    // or an equivalent formula that doesn't have a singularity at center = camera_position:
    //   dot(center - camera_position, cone_axis) >= cone_cutoff * length(center - camera_position) + radius
    //
    // The formula that uses the apex is slightly more accurate but needs the apex; if you are already using bounding sphere
    // to do frustum/occlusion culling, the formula that doesn't use the apex may be preferable.
    if (spec_enable_cone_culling)
    {
        const CullMeshInstanceParams mesh_instance = cull_mesh_instance_params[consts.cull_instance_offset];

        // FIXME only for perspective
        const float3 camera_position_ms = mesh_instance.vs_to_ms_matrix_translate;
        //const bool cone_test = dot(normalize(meshlet.cone_apex_ms - camera_position_ms), meshlet.cone_axis_ms) <= meshlet.cone_cutoff;
        //is_visible = is_visible && cone_test;

        const bool cone_test2 = dot(normalize(meshlet.center_ms - camera_position_ms), meshlet.cone_axis_ms) <= meshlet.cone_cutoff + meshlet.radius / length(meshlet.center_ms - camera_position_ms);
        is_visible = is_visible && cone_test2;
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
