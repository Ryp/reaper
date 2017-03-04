////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_GAMELOGIC_EXPORT_INCLUDED
#define REAPER_GAMELOGIC_EXPORT_INCLUDED

#include "core/Compiler.h"

// Make sure this is up to date with the build system.
#if defined(REAPER_BUILD_SHARED)
    #if defined(reaper_gamelogic_EXPORTS)
        #define REAPER_GAMELOGIC_API REAPER_EXPORT
    #else
        #define REAPER_GAMELOGIC_API REAPER_IMPORT
    #endif
#elif defined(REAPER_BUILD_STATIC)
    #define REAPER_GAMELOGIC_API
#else
    #error Build type must be defined
#endif

#endif // REAPER_GAMELOGIC_EXPORT_INCLUDED
