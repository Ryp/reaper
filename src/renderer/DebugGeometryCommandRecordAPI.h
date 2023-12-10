////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////
// NOTE: Must match with 'command_record_api.hlsl'

#pragma once

#include "RendererExport.h"

#include "renderer/shader/debug_geometry/debug_geometry_public.share.hlsl"

namespace Reaper
{
REAPER_RENDERER_API DebugGeometryUserCommand create_debug_command_sphere(glm::fmat4x3 ms_to_ws_matrix, float radius,
                                                                         u32 color_rgba8_unorm);
} // namespace Reaper
