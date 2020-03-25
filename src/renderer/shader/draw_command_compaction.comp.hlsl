#include "lib/base.hlsl"
#include "lib/indirect_command.hlsl"

#include "share/compaction.hlsl"

//------------------------------------------------------------------------------
// Input

VK_BINDING(0, 0) ByteAddressBuffer DrawCommand;
VK_BINDING(1, 0) ByteAddressBuffer DrawCount;

//------------------------------------------------------------------------------
// Output

VK_BINDING(2, 0) RWByteAddressBuffer DrawCommandOut;
VK_BINDING(3, 0) globallycoherent RWByteAddressBuffer DrawCountOut;

//------------------------------------------------------------------------------

[numthreads(CompactionGroupSize, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    // We could Load() once per wave
    const uint command_count = DrawCount.Load(0);
    const uint input_command_index = dtid.x;

    uint command_index_count = 0;

    if (input_command_index < command_count)
    {
        command_index_count = DrawCommand.Load(input_command_index * IndirectDrawCommandSize * 4);
    }

    const bool is_used = command_index_count > 0;

    const uint is_used_count = WaveActiveCountBits(is_used);
    const uint is_used_prefix_count = WavePrefixCountBits(is_used);

    uint wave_output_command_offset;

    if (WaveIsFirstLane())
    {
        DrawCountOut.InterlockedAdd(0, is_used_count, wave_output_command_offset);
    }

    wave_output_command_offset = WaveReadLaneFirst(wave_output_command_offset);

    if (is_used)
    {
        const uint output_command_index = wave_output_command_offset + is_used_prefix_count - 1;

        // Load the rest of the input command
        const uint4 command_data = DrawCommand.Load4((input_command_index * IndirectDrawCommandSize + 1) * 4);

        DrawCommandOut.Store(output_command_index * IndirectDrawCommandSize * 4, command_index_count);
        DrawCommandOut.Store4((output_command_index * IndirectDrawCommandSize + 1) * 4, command_data);
    }
}
