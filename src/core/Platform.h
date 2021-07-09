////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

// Identify the operating system.
#if defined(WIN32) || defined(_WIN32) || defined(__MINGW32__)
#    define REAPER_PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#elif defined(linux) || defined(__linux) || defined(__linux__)
#    define REAPER_PLATFORM_LINUX
#elif defined(__MACOSX__) || (defined(__APPLE__) && defined(__MACH__)
#    define REAPER_PLATFORM_MACOSX
#else
#    error "Platform not recognised!"
#endif

// Identify CPU architecture
// samples taken from
// http://nadeausoftware.com/articles/2012/02/c_c_tip_how_detect_processor_type_using_compiler_predefined_macros
#if defined(__x86_64__) || defined(_M_X64)
#    define REAPER_CPU_ARCH_X86
#    define REAPER_CPU_ARCH_X86_64
#elif defined(__i386) || defined(_M_IX86)
#    define REAPER_CPU_ARCH_X86
#    define REAPER_CPU_ARCH_X86_32
#else
#    error "CPU not recognised!"
#endif

// NDEBUG must be defined (or not) by the build system
#if !defined(NDEBUG)
#    define REAPER_DEBUG 1
#else
#    define REAPER_DEBUG 0
#endif

constexpr bool isDebugBuild()
{
#if REAPER_DEBUG
    return true;
#else
    return false;
#endif
}
