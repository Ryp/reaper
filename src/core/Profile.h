////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#if defined(REAPER_USE_MICROPROFILE)
#    include "microprofile.h"
#    define REAPER_PROFILE_SCOPE(name) MICROPROFILE_SCOPEI("CPU", name, MP_GREEN)
#    define REAPER_PROFILE_SCOPE_COLOR(name, color) MICROPROFILE_SCOPEI("CPU", name, color)
#    define REAPER_PROFILE_SCOPE_FUNC() MICROPROFILE_SCOPEI("CPU", __func__, MP_GREEN)
#    define REAPER_PROFILE_SCOPE_FUNC_COLOR(color) MICROPROFILE_SCOPEI("CPU", __func__, color)
#    define REAPER_PROFILE_SCOPE_GPU(gpu_log, name, color) MICROPROFILE_SCOPEGPUI_L(gpu_log, name, color)
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
#    define REAPER_PROFILE_SCOPE_GPU(gpu_log, name, color) \
        do                                                 \
        {                                                  \
        } while (0)
#endif
