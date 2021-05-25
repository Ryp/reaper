////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#if defined(REAPER_USE_MICROPROFILE)
#    include "microprofile.h"
#    define REAPER_PROFILE_SCOPE(group, color) MICROPROFILE_SCOPEI(group, __func__, color)
#    define REAPER_PROFILE_SCOPE_GPU(name, color) MICROPROFILE_SCOPEGPUI(name, color)
#else
#    define REAPER_PROFILE_SCOPE(group, color) \
        do                                     \
        {                                      \
        } while (0)
#    define REAPER_PROFILE_SCOPE_GPU(group, color) \
        do                                         \
        {                                          \
        } while (0)
#endif
