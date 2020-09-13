#include "lib/base.hlsl"
#include "lib/indirect_command.hlsl"

#include "share/compaction.hlsl"

VK_BINDING(0, 0) ByteAddressBuffer DrawCount;

VK_BINDING(1, 0) RWByteAddressBuffer IndirectDispatchOut;
VK_BINDING(2, 0) RWByteAddressBuffer DrawCountOut;

[numthreads(1, 1, 1)]
void main()
{
    const uint command_count = DrawCount.Load(0);

    IndirectDispatchCommand dispatchCommand;
    dispatchCommand.x = div_round_up(command_count, CompactionGroupSize);
    dispatchCommand.y = 1;
    dispatchCommand.z = 1;

    // Write indirect dispatch command
    IndirectDispatchOut.Store(0, dispatchCommand.x);
    IndirectDispatchOut.Store(4, dispatchCommand.y);
    IndirectDispatchOut.Store(8, dispatchCommand.z);

    // Reset draw count for the next pass
    DrawCountOut.Store(0, 0);
}
