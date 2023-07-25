////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef VIS_BUFFER_INCLUDED
#define VIS_BUFFER_INCLUDED

#define VisBufferRawType uint2

struct VisibilityBuffer
{
    uint triangle_id;
    uint instance_id;
};

VisBufferRawType encode_vis_buffer(VisibilityBuffer vis_buffer)
{
    return uint2(vis_buffer.triangle_id + 1, vis_buffer.instance_id);
}

VisibilityBuffer decode_vis_buffer(VisBufferRawType vis_buffer_raw)
{
    VisibilityBuffer vis_buffer;

    vis_buffer.triangle_id = vis_buffer_raw.x - 1;
    vis_buffer.instance_id = vis_buffer_raw.y;

    return vis_buffer;
}

bool is_vis_buffer_valid(VisBufferRawType vis_buffer_raw)
{
    return vis_buffer_raw.x != 0;
}

#endif
