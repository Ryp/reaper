////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "profiling/Scope.h"

#include "Debug.h"

namespace Reaper
{
#define REAPER_TOKEN_MERGE0(a, b) a##b
#define REAPER_TOKEN_MERGE(a, b) REAPER_TOKEN_MERGE0(a, b)

// Inserts a CPU and GPU profile scope, as well as a region marker
#define REAPER_GPU_SCOPE_COLOR(command_buffer, name, color)                                               \
    VulkanDebugLabelCmdBufferScope REAPER_TOKEN_MERGE(gpu_scope_, __LINE__)(command_buffer.handle, name); \
    REAPER_PROFILE_SCOPE_GPU_COLOR(command_buffer, name, color);                                          \
    REAPER_PROFILE_SCOPE_COLOR(name, color);

#define REAPER_GPU_SCOPE(command_buffer, name)                                                            \
    VulkanDebugLabelCmdBufferScope REAPER_TOKEN_MERGE(gpu_scope_, __LINE__)(command_buffer.handle, name); \
    REAPER_PROFILE_SCOPE_GPU(command_buffer, name);                                                       \
    REAPER_PROFILE_SCOPE(name);
} // namespace Reaper
