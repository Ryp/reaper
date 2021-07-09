////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#if defined(REAPER_USE_MICROPROFILE)
#    include "microprofile.h"
#    define REAPER_PROFILE_SCOPE(group, color) MICROPROFILE_SCOPEI(group, __func__, color)
#    define REAPER_PROFILE_SCOPE_GPU(gpu_log, name, color) MICROPROFILE_SCOPEGPUI_L(gpu_log, name, color)
#else
#    define REAPER_PROFILE_SCOPE(group, color) \
        do                                     \
        {                                      \
        } while (0)
#    define REAPER_PROFILE_SCOPE_GPU(gpu_log, group, color) \
        do                                                  \
        {                                                   \
        } while (0)
#endif
