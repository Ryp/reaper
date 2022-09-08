////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_INDIRECT_COMMAND_INCLUDED
#define LIB_INDIRECT_COMMAND_INCLUDED

// For vkCmdDrawIndirect
struct DrawIndirectCommand
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

// For vkCmdDrawIndexedIndirect
struct DrawIndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};

// For vkCmdDispatchIndirect
struct DispatchIndirectCommand
{
    uint x;
    uint y;
    uint z;
};

uint div_round_up(uint batch_size, uint group_size)
{
    return (batch_size + group_size - 1) / group_size;
}

#endif
