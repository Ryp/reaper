#include "lib/base.hlsl"
#include "lib/indirect_command.hlsl"
#include "lib/vertex_pull.hlsl"

#include "share/tiled_lighting/raster.hlsl"
#include "tiled_lighting.hlsl"

VK_PUSH_CONSTANT_HELPER(ClassifyVolumePushConstants) consts;

VK_BINDING(0, 0) ByteAddressBuffer VertexPositionsMS;
VK_BINDING(1, 0) globallycoherent RWByteAddressBuffer InnerOuterCounter;
VK_BINDING(2, 0) RWStructuredBuffer<DrawIndirectCommand> DrawCommandsInner;
VK_BINDING(3, 0) RWStructuredBuffer<DrawIndirectCommand> DrawCommandsOuter;
VK_BINDING(4, 0) StructuredBuffer<ProxyVolumeInstance> ProxyVolumeBuffer;

groupshared uint lds_is_inner;

[numthreads(ClassifyVolumeThreadCount, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    if (gi == 0)
    {
        lds_is_inner = false;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint vertex_id = dtid.x + consts.vertex_offset;
    const uint instance_id = dtid.y + consts.instance_id_offset;

    const ProxyVolumeInstance proxy_volume = ProxyVolumeBuffer[instance_id];
    const float3 vpos_ms = pull_position(VertexPositionsMS, vertex_id);
    const float3 vpos_vs = mul(proxy_volume.ms_to_vs_with_scale, float4(vpos_ms, 1.0)); // FIXME handle scale properly

    const bool is_active = dtid.x < consts.vertex_count;
    const bool is_inner = vpos_vs.z >= consts.near_clip_plane_depth_vs;

    // FIXME should only increment once per gid.y!
    if (is_active && is_inner)
    {
        InterlockedOr(lds_is_inner, true);
    }

    GroupMemoryBarrierWithGroupSync();

    if (gi == 0)
    {
        uint mesh_offset;

        DrawIndirectCommand command;
        command.vertexCount = consts.vertex_count;
        command.instanceCount = 1;
        command.firstVertex = consts.vertex_offset;
        command.firstInstance = instance_id;

        if (lds_is_inner)
        {
            InnerOuterCounter.InterlockedAdd(InnerCounterOffsetBytes, 1u, mesh_offset);
            DrawCommandsInner[mesh_offset] = command;
        }
        else
        {
            InnerOuterCounter.InterlockedAdd(OuterCounterOffsetBytes, 1u, mesh_offset);
            DrawCommandsOuter[mesh_offset] = command;
        }
    }
}
