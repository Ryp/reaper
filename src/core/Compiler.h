////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_COMPILER_INCLUDED
#define REAPER_COMPILER_INCLUDED

#if defined(__INTEL_COMPILER) || defined(__ICC)
    #define REAPER_COMPILER_ICC
#elif defined(__GNUC__)
    #define REAPER_COMPILER_GCC
#elif defined(__clang__)
    #define REAPER_COMPILER_CLANG
#elif defined(_MSC_VER)
    #define REAPER_COMPILER_MSVC
#else
    #error Compiler not recognised!
#endif

#endif // REAPER_COMPILER_INCLUDED
