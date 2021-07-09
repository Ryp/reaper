////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Assert.h"

#include "Debugger.h"
#include "Platform.h"
#include "StackTrace.h"

#include <iostream>

#if defined(REAPER_PLATFORM_WINDOWS)
#    include <intrin.h> // for __debugbreak
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
#    error "breakpoint() not available!"
#endif
}

void AssertImpl(const char* file, const char* func, int line, bool condition, const std::string& message)
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
