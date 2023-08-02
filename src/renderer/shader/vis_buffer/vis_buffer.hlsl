////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef VIS_BUFFER_INCLUDED
#define VIS_BUFFER_INCLUDED

#include "lib/format/bitfield.hlsl"

struct VisibilityBuffer
{
    uint visible_meshlet_index;
    uint meshlet_triangle_id;
};

#define VisBufferRawType uint

static const uint VisibleMeshletIndex_Offset = 0;
static const uint VisibleMeshletIndex_Bits = 25;
static const uint MeshletTriangleIndex_Offset = VisibleMeshletIndex_Bits;
static const uint MeshletTriangleIndex_Bits = 7;

// NOTE: Overflow is not handled
VisBufferRawType encode_vis_buffer(VisibilityBuffer vis_buffer)
{
    return (vis_buffer.visible_meshlet_index + 1) << VisibleMeshletIndex_Offset
          | vis_buffer.meshlet_triangle_id << MeshletTriangleIndex_Offset;
}

VisibilityBuffer decode_vis_buffer(VisBufferRawType vis_buffer_raw)
{
    VisibilityBuffer vis_buffer;

    vis_buffer.visible_meshlet_index = bitfield_extract(vis_buffer_raw, VisibleMeshletIndex_Offset, VisibleMeshletIndex_Bits) - 1;
    vis_buffer.meshlet_triangle_id = bitfield_extract(vis_buffer_raw, MeshletTriangleIndex_Offset, MeshletTriangleIndex_Bits);

    return vis_buffer;
}

bool is_vis_buffer_valid(VisBufferRawType vis_buffer_raw)
{
    return vis_buffer_raw != 0;
}

#endif
