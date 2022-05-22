#include "lib/base.hlsl"
#include "lib/indirect_command.hlsl"

#include "share/meshlet_culling.hlsl"

VK_BINDING(0, 0) ByteAddressBuffer Counters;
VK_BINDING(1, 0) RWStructuredBuffer<IndirectDispatchCommand> IndirectDispatchOut;

[numthreads(PrepareIndirectDispatchThreadCount, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    const uint pass_index = dtid.x;
    const uint counter_offset = pass_index * CountersCount + MeshletCounterOffset;

    const uint meshlet_count = Counters.Load(counter_offset * 4);

    IndirectDispatchCommand dispatchCommand;
    dispatchCommand.x = meshlet_count;
    dispatchCommand.y = 1;
    dispatchCommand.z = 1;

    // Write indirect dispatch command
    IndirectDispatchOut[pass_index] = dispatchCommand;
}