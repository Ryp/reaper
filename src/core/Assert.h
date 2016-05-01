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

#if defined(REAPER_PLATFORM_WINDOWS)
    #include <intrin.h> // for __debugbreak
#endif

inline void breakpoint()
{
#if defined(REAPER_PLATFORM_WINDOWS)
    __debugbreak();
#elif defined(REAPER_PLATFORM_LINUX) && defined(REAPER_CPU_ARCH_X86)
    __asm__("int $3");
#elif defined(REAPER_PLATFORM_MACOSX) && defined(REAPER_CPU_ARCH_X86)
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

// Macro overloading code (yuck)
// http://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
// http://stackoverflow.com/questions/9183993/msvc-variadic-macro-expansion

#define GLUE(x, y) x y

#define RETURN_ARG_COUNT(_1_, _2_, _3_, _4_, _5_, count, ...) count
#define EXPAND_ARGS(args) RETURN_ARG_COUNT args
#define COUNT_ARGS_MAX5(...) EXPAND_ARGS((__VA_ARGS__, 5, 4, 3, 2, 1, 0))

#define OVERLOAD_MACRO2(name, count) name##count
#define OVERLOAD_MACRO1(name, count) OVERLOAD_MACRO2(name, count)
#define OVERLOAD_MACRO(name, count) OVERLOAD_MACRO1(name, count)

#define CALL_OVERLOAD(name, ...) GLUE(OVERLOAD_MACRO(name, COUNT_ARGS_MAX5(__VA_ARGS__)), (__VA_ARGS__))

#define Assert1(condition) AssertImpl(condition, __FILE__, __FUNCTION__, __LINE__)
#define Assert2(condition, message) AssertImpl(condition, __FILE__, __FUNCTION__, __LINE__, message)

// Actual macros
#define Assert(...) CALL_OVERLOAD(Assert, __VA_ARGS__)
#define AssertUnreachable() AssertImpl(false, __FILE__, __FUNCTION__, __LINE__, "unreachable")

#endif // REAPER_ASSERT_INCLUDED
