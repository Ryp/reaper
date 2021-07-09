////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "StackTrace.h"

#if defined(REAPER_PLATFORM_LINUX)

#    define UNW_LOCAL_ONLY
#    include <cxxabi.h> // This may be invalid on windows
#    include <libunwind.h>

#    include <iomanip>
#    include <iostream>

void printStacktrace()
{
    unw_cursor_t  cursor;
    unw_context_t context;
    unw_word_t    offset;
    unw_word_t    pc;
    static char   sym[256];

    // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    // Unwind frames one by one, going up the frame stack.
    while (unw_step(&cursor) > 0)
    {
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0)
            break;
        // TODO detect 32bit platform and fill with 8
        std::cerr << "0x" << std::hex << std::setfill('0') << std::setw(16) << pc << ": ";

        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0)
        {
            char* nameptr = sym;
            int   status;
            char* demangled = abi::__cxa_demangle(sym, nullptr, nullptr, &status);

            if (status == 0)
                nameptr = demangled;
            std::cerr << nameptr << " (+0x" << offset << ")" << std::endl;
            std::free(demangled);
        }
        else
            std::cerr << " unknown" << std::endl;
    }
}

#elif defined(REAPER_PLATFORM_WINDOWS) || defined(REAPER_PLATFORM_MACOSX)

void printStacktrace()
{
    AssertUnreachable();
}

#else
#    error "printStacktrace() not available!"
#endif
