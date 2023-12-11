////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DebugGeometryCommandRecordAPI.h"

namespace Reaper
{
DebugGeometryUserCommand create_debug_command_sphere(glm::fmat4x3 ms_to_ws_matrix, float radius, u32 color_rgba8_unorm)
{
    return DebugGeometryUserCommand{
        .ms_to_ws_matrix = ms_to_ws_matrix,
        .geometry_type = DebugGeometryType_Icosphere,
        .color_rgba8_unorm = color_rgba8_unorm,
        ._pad0 = 0,
        ._pad1 = 0,
        .half_extent = glm::fvec3(radius, radius, radius),
        ._pad2 = 0,
    };
}

REAPER_RENDERER_API DebugGeometryUserCommand create_debug_command_box(glm::fmat4x3 ms_to_ws_matrix,
                                                                      glm::fvec3 half_extent, u32 color_rgba8_unorm)
{
    return DebugGeometryUserCommand{
        .ms_to_ws_matrix = ms_to_ws_matrix,
        .geometry_type = DebugGeometryType_Box,
        .color_rgba8_unorm = color_rgba8_unorm,
        ._pad0 = 0,
        ._pad1 = 0,
        .half_extent = half_extent,
        ._pad2 = 0,
    };
}
} // namespace Reaper
