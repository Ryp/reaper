////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_PLATFORM_INCLUDED
#define REAPER_PLATFORM_INCLUDED

// Identify the operating system.
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__MINGW32__)
    #define REAPERGL_PLATFORM_WINDOWS
#elif defined(linux) || defined(__linux) || defined(__linux__)
    #define REAPERGL_PLATFORM_LINUX
#elif defined(__APPLE__) || defined(__MACH__)
    #define REAPERGL_PLATFORM_MACOSX
#else
    #error Platform not recognised!
#endif

// Identify CPU architecture
// samples taken from http://nadeausoftware.com/articles/2012/02/c_c_tip_how_detect_processor_type_using_compiler_predefined_macros
#if defined(__x86_64__) || defined(_M_X64)
    #define REAPERGL_CPU_ARCH_X86
    #define REAPERGL_CPU_ARCH_X86_64
#elif defined(__i386) || defined(_M_IX86)
    #define REAPERGL_CPU_ARCH_X86
    #define REAPERGL_CPU_ARCH_X86_32
#else
    #error CPU not recognised!
#endif

// Define portable import/export macros.
#if defined(REAPERGL_PLATFORM_WINDOWS)
    #define REAPERGL_EXPORT __declspec(dllexport)
    #define REAPERGL_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
    #define REAPERGL_EXPORT __attribute__((visibility("hidden")))
    #define REAPERGL_IMPORT
#else
    #define REAPERGL_EXPORT
    #define REAPERGL_IMPORT
#endif

// NDEBUG must be defined (or not) by the build system
#if !defined(NDEBUG)
    #define DEBUG
#endif

constexpr bool isDebugBuild()
{
#if defined(DEBUG)
    return true;
#else
    return false;
#endif
}

#endif // REAPER_PLATFORM_INCLUDED
