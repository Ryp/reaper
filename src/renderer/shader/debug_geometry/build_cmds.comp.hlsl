////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "lib/base.hlsl"
#include "lib/format/unorm.hlsl"
#include "lib/indirect_command.hlsl"

#include "share/debug_geometry_private.hlsl"

VK_BINDING(0, 0) ConstantBuffer<DebugGeometryBuildCmdsPassConstants> PassConstants;
VK_BINDING(1, 0) ByteAddressBuffer DrawCounter;
VK_BINDING(2, 0) StructuredBuffer<DebugGeometryUserCommand> UserCommands;

VK_BINDING(3, 0) RWStructuredBuffer<DrawIndexedIndirectCommand> DrawCommands;
VK_BINDING(4, 0) RWStructuredBuffer<DebugGeometryInstance> InstanceBuffer;

float4x4 float3x4_to_float4x4(float3x4 m)
{
    return float4x4(
        m, float4(0, 0, 0, 1)
    );
}

[numthreads(DebugGeometryBuildCmdsThreadCount, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    const uint instance_id = dtid.x;
    const uint instance_count = DrawCounter.Load(0);

    if (instance_id < instance_count)
    {
        const DebugGeometryUserCommand user_command = UserCommands[instance_id];

        const DebugGeometryMeshAlloc mesh_alloc = PassConstants.debug_geometry_allocs[user_command.geometry_type];

        DrawIndexedIndirectCommand command;
        command.indexCount = mesh_alloc.index_count;
        command.instanceCount = 1;
        command.firstIndex = mesh_alloc.index_offset;
        command.vertexOffset = mesh_alloc.vertex_offset;
        command.firstInstance = instance_id;

        DrawCommands[instance_id] = command;

        DebugGeometryInstance instance;
        instance.ms_to_cs = float3x4_to_float4x4(user_command.ms_to_ws_matrix) * PassConstants.main_camera_ws_to_cs;
        instance.color = rgba8_unorm_to_rgba32_float(user_command.color_rgba8_unorm);
        instance.radius = user_command.radius;

        InstanceBuffer[instance_id] = instance;
    }
}
