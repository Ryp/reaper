////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef DEBUG_GEOMETRY_COMMAND_RECORD_API_INCLUDED
#define DEBUG_GEOMETRY_COMMAND_RECORD_API_INCLUDED

#include "debug_geometry_public.share.hlsl"

DebugGeometryUserCommand create_debug_command_sphere(float3x4 ms_to_ws_matrix, float radius, uint color_rgba8_unorm)
{
    DebugGeometryUserCommand command;

    command.ms_to_ws_matrix = ms_to_ws_matrix;
    command.geometry_type = DebugGeometryType_Icosphere;
    command.radius = radius;
    command.color_rgba8_unorm = color_rgba8_unorm;

    return command;
}

// Since HLSL and SPIR-V don't play well together when dealing with passing buffers to functions,
// you might have to inline the content of this function.
void append_debug_geometry(globallycoherent RWByteAddressBuffer counter_buffer,
                           RWStructuredBuffer<DebugGeometryUserCommand> command_buffer,
                           DebugGeometryUserCommand command)
{
    uint command_index;

    const uint counter_offset_bytes = 0;
    counter_buffer.InterlockedAdd(counter_offset_bytes, 1u, command_index);

    command_buffer[command_index] = command;
}

#endif
