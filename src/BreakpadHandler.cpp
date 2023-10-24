////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "BreakpadHandler.h"

#include <core/Assert.h>
#include <core/Platform.h>

#include "handler/exception_handler.h"

#if defined(REAPER_PLATFORM_LINUX)
static bool dump_callback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded)

{
    static_cast<void>(context);
    static_cast<void>(descriptor);

    return succeeded;
}
#elif defined(REAPER_PLATFORM_WINDOWS)
#    include <tchar.h>
constexpr size_t  kMaximumLineLength = 256;
constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\BreakpadCrashServices\\TestServer";
static size_t     kCustomInfoCount = 2;

static google_breakpad::CustomInfoEntry kCustomInfoEntries[] = {
    {L"prod", L"CrashTestApp"},
    {L"ver", L"1.0"},
};

bool ShowDumpResults(const wchar_t*      dump_path,
                     const wchar_t*      minidump_id,
                     void*               context,
                     EXCEPTION_POINTERS* exinfo,
                     MDRawAssertionInfo* assertion,
                     bool                succeeded)
{
    return succeeded;
}
#endif

namespace Reaper
{
BreakpadContext create_breakpad_context()
{
#if defined(REAPER_PLATFORM_LINUX)
    const char* dump_path = "/tmp";
#elif defined(REAPER_PLATFORM_WINDOWS)
    const wchar_t*                    dump_path = L"C:\\dumps\\";
#endif

#if defined(REAPER_PLATFORM_LINUX)
    google_breakpad::MinidumpDescriptor descriptor(dump_path);

    auto* handler = new google_breakpad::ExceptionHandler(descriptor, nullptr, dump_callback, nullptr, true, 1);
#elif defined(REAPER_PLATFORM_WINDOWS)
    google_breakpad::CustomClientInfo custom_info = {kCustomInfoEntries, kCustomInfoCount};

    auto* handler = new google_breakpad::ExceptionHandler(dump_path,
                                                          nullptr,
                                                          ShowDumpResults,
                                                          nullptr,
                                                          google_breakpad::ExceptionHandler::HANDLER_ALL,
                                                          MiniDumpNormal,
                                                          kPipeName,
                                                          &custom_info);
#endif

    return BreakpadContext{.handler = handler};
}

void destroy_breakpad_context(const BreakpadContext& breakpad_context)
{
    Assert(breakpad_context.handler);

    if (breakpad_context.handler)
    {
        delete breakpad_context.handler;
    }
}
} // namespace Reaper
