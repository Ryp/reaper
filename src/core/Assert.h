////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_ASSERT_INCLUDED
#define REAPER_ASSERT_INCLUDED

#include <iostream>
#include <string>

#include "Platform.h"
#include "Debugger.h"

#if defined(REAPERGL_PLATFORM_WINDOWS)
    #include <intrin.h> // for __debugbreak
#endif

inline void breakpoint()
{
#if defined(REAPERGL_PLATFORM_WINDOWS)
    __debugbreak();
#elif defined(REAPERGL_PLATFORM_LINUX) && defined(REAPERGL_CPU_ARCH_X86)
    __asm__("int $3");
#elif defined(REAPERGL_PLATFORM_MACOSX) && defined(REAPERGL_CPU_ARCH_X86)
    __asm__("int $3");
#else
    #error breakpoint() not available!
#endif
}

#include "StackTrace.h"

inline void AssertImpl(bool condition, const char* file, const char* func, int line, const std::string& message = std::string())
{
    if (condition)
        return;
    std::cerr << std::dec << "ASSERT FAILED " << file << ':' << line << ": in '" << func << "'" << std::endl;
    if (isInDebugger())
        breakpoint();
    else
        printStacktrace();

    if (!message.empty())
        std::cerr << "ASSERT MESSAGE " << message << std::endl;
}

#define Assert1(condition) AssertImpl(condition, __FILE__, __FUNCTION__, __LINE__)
#define Assert2(condition, message) AssertImpl(condition, __FILE__, __FUNCTION__, __LINE__, message)
#define AssertUnreachable() AssertImpl(false, __FILE__, __FUNCTION__, __LINE__)

// http://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
#define OVERLOADED_MACRO(_1, _2, NAME, ...) NAME
#define Assert(...) OVERLOADED_MACRO(__VA_ARGS__, Assert2, Assert1) (__VA_ARGS__)

#endif // REAPER_ASSERT_INCLUDED
