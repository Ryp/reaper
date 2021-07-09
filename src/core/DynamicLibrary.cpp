////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DynamicLibrary.h"

#if defined(REAPER_PLATFORM_LINUX) || defined(REAPER_PLATFORM_MACOSX)

#    include <dlfcn.h>

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
} // namespace dynlib

#elif defined(REAPER_PLATFORM_WINDOWS)

namespace dynlib
{
static std::string getErrorString()
{
    // Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
        return std::string(); // No error message has been recorded

    LPSTR  messageBuffer = nullptr;
    size_t size =
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    // Free the buffer.
    LocalFree(messageBuffer);

    return message;
}

LibHandle load(const std::string& library)
{
    HMODULE handle = nullptr;

    handle = LoadLibrary(library.c_str());
    Assert(handle != nullptr, getErrorString());

    return handle;
}

LibSym getSymbol(LibHandle handle, const std::string& name)
{
    FARPROC sym = nullptr;

    sym = GetProcAddress(handle, name.c_str());
    Assert(sym != nullptr, getErrorString());

    return sym;
}

void close(LibHandle handle)
{
    FreeLibrary(handle);
}
} // namespace dynlib

#endif
