#include "lib/base.hlsl"
#include "lib/indirect_command.hlsl"

#include "share/culling.hlsl"

//------------------------------------------------------------------------------
// Input

struct CullMeshletPushConstants
{
    hlsl_uint meshlet_count;
    hlsl_uint mesh_instance_count;
    hlsl_uint firstCullInstance;
    hlsl_uint _pad1;
};

// https://github.com/KhronosGroup/glslang/issues/1629
#if defined(_DXC)
VK_PUSH_CONSTANT() CullMeshletPushConstants consts;
#else
VK_PUSH_CONSTANT() ConstantBuffer<CullMeshletPushConstants> consts;
#endif

VK_BINDING(0, 0) ConstantBuffer<CullPassParams> pass_params;
VK_BINDING(1, 0) StructuredBuffer<CullMeshInstanceParams> cull_mesh_instance_params;
VK_BINDING(2, 0) StructuredBuffer<CullMeshletInstanceParams> cull_meshlet_instance_params;

//------------------------------------------------------------------------------
// Output

VK_BINDING(3, 0) RWByteAddressBuffer MeshletDrawCommandOut;
VK_BINDING(4, 0) globallycoherent RWByteAddressBuffer MeshletCountOut;

//------------------------------------------------------------------------------

// FIXME find trade-off between instance count and meshlet count
[numthreads(16, 4, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    uint meshlet_index = dtid.x;
    uint mesh_instance_index = dtid.y;

    bool is_visible = false;

    if (meshlet_index < consts.meshlet_count && mesh_instance_index < consts.mesh_instance_count)
    {
        const CullMeshletInstanceParams meshlet_instance = cull_meshlet_instance_params[meshlet_index];
        const CullMeshInstanceParams mesh_instance = cull_mesh_instance_params[mesh_instance_index];

        const uint cull_instance_id = consts.firstCullInstance + mesh_instance_index;
        const float4x4 ms_to_cs_matrix = cull_mesh_instance_params[cull_instance_id].ms_to_cs_matrix;

        is_visible = ((meshlet_index % 2) == 0);
    }

//////////////////////////////////////////////////////////////
//     // Compaction part
//     const uint is_visible_count = WaveActiveCountBits(is_used);
//     const uint is_visible_prefix_count = WavePrefixCountBits(is_used);
//
//     uint wave_output_command_offset;
//
//     if (WaveIsFirstLane())
//     {
//         DrawCountOut.InterlockedAdd(0, is_visible_count, wave_output_command_offset);
//     }
//
//     wave_output_command_offset = WaveReadLaneFirst(wave_output_command_offset);
//
//     if (is_visible)
//     {
//         const uint output_command_index = wave_output_command_offset + is_visible_prefix_count - 1;
//
//         // Load the rest of the input command
//         const uint4 command_data = DrawCommand.Load4((input_command_index * IndirectDrawCommandSize + 1) * 4);
//
//         DrawCommandOut.Store(output_command_index * IndirectDrawCommandSize * 4, command_index_count);
//         DrawCommandOut.Store4((output_command_index * IndirectDrawCommandSize + 1) * 4, command_data);
//     }
// }
}
