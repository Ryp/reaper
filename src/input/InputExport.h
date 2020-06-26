////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// Generated by CMake

#pragma once

#include "core/Compiler.h"

#if defined(REAPER_BUILD_SHARED)
#    if defined(REAPER_INPUT_EXPORT)
#        define REAPER_INPUT_API REAPER_EXPORT
#    else
#        define REAPER_INPUT_API REAPER_IMPORT
#    endif
#elif defined(REAPER_BUILD_STATIC)
#    define REAPER_INPUT_API
#else
#    error "Build type must be defined"
#endif
