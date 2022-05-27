////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "core/Profile.h"

#include "Debug.h"

namespace Reaper
{
// Inserts a CPU and GPU profile scope, as well as a region marker
#define REAPER_GPU_SCOPE_COLOR(command_buffer, name, color)                                \
    VulkanDebugLabelCmdBufferScope vk_scope_region##__LINE__(command_buffer.handle, name); \
    REAPER_PROFILE_SCOPE_GPU(command_buffer.mlog, name, color);                            \
    REAPER_PROFILE_SCOPE_COLOR(name, color);

#define REAPER_GPU_SCOPE(command_buffer, name) REAPER_GPU_SCOPE_COLOR(command_buffer, name, MP_GOLD)
} // namespace Reaper
