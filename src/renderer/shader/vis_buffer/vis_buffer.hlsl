////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef VIS_BUFFER_INCLUDED
#define VIS_BUFFER_INCLUDED

#include "lib/format/bitfield.hlsl"

#define VisBufferRawType uint

// FIXME this is not a lot of bits for the instance id
static const uint TriangleIDOffset = 0;
static const uint TriangleIDBits = 22;
static const uint InstanceIDOffset = TriangleIDBits;
static const uint InstanceIDBits = 10;

struct VisibilityBuffer
{
    uint triangle_id;
    uint instance_id;
};

// NOTE: Overflow is not handled
VisBufferRawType encode_vis_buffer(VisibilityBuffer vis_buffer)
{
    return (vis_buffer.triangle_id + 1) << TriangleIDOffset
          | vis_buffer.instance_id << InstanceIDOffset;
}

VisibilityBuffer decode_vis_buffer(VisBufferRawType vis_buffer_raw)
{
    VisibilityBuffer vis_buffer;

    vis_buffer.triangle_id = bitfield_extract(vis_buffer_raw, TriangleIDOffset, TriangleIDBits) - 1;
    vis_buffer.instance_id = bitfield_extract(vis_buffer_raw, InstanceIDOffset, InstanceIDBits);

    return vis_buffer;
}

bool is_vis_buffer_valid(VisBufferRawType vis_buffer_raw)
{
    return vis_buffer_raw != 0;
}

#endif
