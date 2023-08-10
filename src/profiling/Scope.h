////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <core/HexaColorConstants.h>

#if defined(REAPER_USE_TRACY)
#    include <vulkan_loader/Vulkan.h>

#    include "tracy/Tracy.hpp"
#    include "tracy/TracyVulkan.hpp"

#    define REAPER_PROFILE_SCOPE(name) ZoneScopedN(name)
#    define REAPER_PROFILE_SCOPE_COLOR(name, color) ZoneScopedNC(name, color)
#    define REAPER_PROFILE_SCOPE_FUNC() ZoneScoped
#    define REAPER_PROFILE_SCOPE_FUNC_COLOR(color) ZoneScopedC(color)
#    define REAPER_PROFILE_SCOPE_GPU(cmd_buffer, name) TracyVkZone(cmd_buffer.tracy_ctx, cmd_buffer.handle, name)
#    define REAPER_PROFILE_SCOPE_GPU_COLOR(cmd_buffer, name, color) \
        TracyVkZoneC(cmd_buffer.tracy_ctx, cmd_buffer.handle, name, color)
#else
#    define REAPER_PROFILE_SCOPE(name) \
        do                             \
        {                              \
        } while (0)
#    define REAPER_PROFILE_SCOPE_COLOR(name, color) \
        do                                          \
        {                                           \
        } while (0)
#    define REAPER_PROFILE_SCOPE_FUNC() \
        do                              \
        {                               \
        } while (0)
#    define REAPER_PROFILE_SCOPE_FUNC_COLOR(color) \
        do                                         \
        {                                          \
        } while (0)
#    define REAPER_PROFILE_SCOPE_GPU(cmd_buffer, name) \
        do                                             \
        {                                              \
        } while (0)
#    define REAPER_PROFILE_SCOPE_GPU_COLOR(cmd_buffer, name, color) \
        do                                                          \
        {                                                           \
        } while (0)
#endif
