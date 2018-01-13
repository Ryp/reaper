////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// Generated by CMake

#pragma once

#include "core/Compiler.h"

#if defined(REAPER_BUILD_SHARED)
#    if defined(reaper_mesh_EXPORTS)
#        define REAPER_MESH_API REAPER_EXPORT
#    else
#        define REAPER_MESH_API REAPER_IMPORT
#    endif
#elif defined(REAPER_BUILD_STATIC)
#    define REAPER_MESH_API
#else
#    error "Build type must be defined"
#endif
