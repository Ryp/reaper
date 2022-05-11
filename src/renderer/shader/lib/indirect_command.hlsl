////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_INDIRECT_COMMAND_INCLUDED
#define LIB_INDIRECT_COMMAND_INCLUDED

static const uint IndirectDrawCommandSize = 5;

struct IndirectDrawCommand
{
    uint indexCount;
    uint instance_count;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};

static const uint IndirectDispatchCommandSize = 3;

struct IndirectDispatchCommand
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
