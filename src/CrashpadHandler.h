////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#include <core/Platform.h>
#include <core/Types.h>

namespace crashpad
{
class CrashpadClient;
}

namespace Reaper
{
struct CrashpadConfig
{
    bool        enable_crash_upload = false;         // Can be set in INI
    std::string upload_server_address = "localhost"; // Can be set in INI
    u32         upload_server_port = 30934;          // Can be set in INI
    bool        restartable = true;
    bool        asynchronous_start = false;
    // NOTE: Only for testing purposes
    bool trigger_crash_at_startup = false; // Can be set in INI

    // FIXME Handle Unicode strings better
#if defined(REAPER_PLATFORM_LINUX)
    std::string crash_handler_path;
    std::string dumps_path;
#elif defined(REAPER_PLATFORM_WINDOWS)
    std::wstring crash_handler_path;
    std::wstring dumps_path;
#endif
};

struct CrashpadContext
{
    crashpad::CrashpadClient* client;
    bool                      is_started;
};

CrashpadContext create_crashpad_context(const CrashpadConfig& config);
void            destroy_crashpad_context(const CrashpadContext& crashpad_context);

CrashpadConfig crashpad_parse_ini_config(const char* filename);
} // namespace Reaper
