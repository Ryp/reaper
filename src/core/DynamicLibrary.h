////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_DYNAMICLIBRARY_INCLUDED
#define REAPER_DYNAMICLIBRARY_INCLUDED

#include <string>

#if defined(REAPER_PLATFORM_LINUX) || defined(REAPER_PLATFORM_MACOSX)
    using LibHandle = void*;
    using LibSym = void*;
#elif defined(REAPER_PLATFORM_WINDOWS)
    using LibHandle = HMODULE;
    using LibSym = FARPROC;
#endif

namespace dynlib
{
    REAPER_CORE_API LibHandle load(const std::string& library);
    REAPER_CORE_API LibSym getSymbol(LibHandle handle, const std::string& name);
    REAPER_CORE_API void close(LibHandle handle);
}

#endif // REAPER_DYNAMICLIBRARY_INCLUDED
