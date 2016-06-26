////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_CORE_EXPORT_INCLUDED
#define REAPER_CORE_EXPORT_INCLUDED

#include "core/Compiler.h"

// Make sure this is up to date with the build system.
#if defined(reaper_core_EXPORTS)
    #define REAPER_CORE_API REAPER_EXPORT
#else
    #define REAPER_CORE_API REAPER_IMPORT
#endif

#endif // REAPER_CORE_EXPORT_INCLUDED
