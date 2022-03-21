////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// Generated by CMake

#pragma once

#include "core/Compiler.h"

#if defined(REAPER_BUILD_SHARED)
#    if defined(REAPER_GAMELOGIC_EXPORT)
#        define REAPER_GAMELOGIC_API REAPER_EXPORT
#    else
#        define REAPER_GAMELOGIC_API REAPER_IMPORT
#    endif
#elif defined(REAPER_BUILD_STATIC)
#    define REAPER_GAMELOGIC_API
#else
#    error "Build type must be defined"
#endif
