////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// Generated by CMake

#pragma once

#include "core/Compiler.h"

#if defined(REAPER_BUILD_SHARED)
#    if defined(SPLINESONIC_TRACKGEN_EXPORT)
#        define SPLINESONIC_TRACKGEN_API REAPER_EXPORT
#    else
#        define SPLINESONIC_TRACKGEN_API REAPER_IMPORT
#    endif
#elif defined(REAPER_BUILD_STATIC)
#    define SPLINESONIC_TRACKGEN_API
#else
#    error "Build type must be defined"
#endif
