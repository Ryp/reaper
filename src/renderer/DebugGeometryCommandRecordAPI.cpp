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
        .radius = radius,
        ._pad = 0,
    };
}
} // namespace Reaper
