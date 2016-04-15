////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DynamicLibrary.h"

#if defined(REAPERGL_PLATFORM_LINUX) || defined(REAPERGL_PLATFORM_MACOSX)

#include <dlfcn.h>

namespace dynlib
{
    static std::string getErrorString()
    {
        const char* str = dlerror();
        return str ? str : "Unknown error";
    }

    LibHandle load(const std::string& library)
    {
        void* handle = nullptr;

        handle = dlopen(library.c_str(), RTLD_NOW);
        Assert(handle != nullptr, getErrorString());

        return handle;
    }

    LibSym getSymbol(LibHandle handle, const std::string& name)
    {
        void* sym = nullptr;

        sym = dlsym(handle, name.c_str());
        Assert(sym != nullptr, getErrorString());

        return sym;
    }

    void close(LibHandle handle)
    {
        int ret = dlclose(handle);
        Assert(!ret, getErrorString());
    }
}

#elif defined(REAPERGL_PLATFORM_WINDOWS)

namespace dynlib
{
    LibHandle load(const std::string& /*library*/)
    {
        AssertUnreachable();
    }

    LibSym getSymbol(LibHandle /*handle*/, const std::string& /*name*/)
    {
        AssertUnreachable();
    }

    void close(LibHandle /*handle*/)
    {
        AssertUnreachable();
    }
}

#endif
