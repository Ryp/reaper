////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "CrashpadHandler.h"

#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"
#include "client/simulate_crash.h"

#include <core/Assert.h>
#include <core/Platform.h>

#include <core/Crash.h>

#include <fmt/format.h>
#include <ini.h>

namespace Reaper
{
namespace
{
    std::string config_get_upload_url(const CrashpadConfig& config)
    {
        return fmt::format("http://{}:{}", config.upload_server_address, config.upload_server_port);
    }
} // namespace

CrashpadContext create_crashpad_context(const CrashpadConfig& config)
{
    // FIXME
#if defined(REAPER_PLATFORM_LINUX)
    auto database_dir = base::FilePath("/tmp");
    auto handler_path = base::FilePath("build/external/crashpad/crashpad_handler");
#elif defined(REAPER_PLATFORM_WINDOWS)
    auto database_dir = base::FilePath(L"C:\\tmp");
    auto handler_path = base::FilePath(L"crashpad_handler.exe");
#endif

    auto metrics_dir = database_dir;

    auto database = crashpad::CrashReportDatabase::Initialize(database_dir);

    if (database == nullptr)
    {
        return CrashpadContext{.client = nullptr, .is_started = false};
    }

    Assert(database->GetSettings()->SetUploadsEnabled(config.enable_crash_upload));

    auto* client = new crashpad::CrashpadClient();

    auto upload_url = config_get_upload_url(config);

    // NOTE: Crash has a low rate limit to be the least intrusive possible.
    // We can disable it for now.
    bool status = client->StartHandler(handler_path, database_dir, metrics_dir, upload_url, {}, {"--no-rate-limit"},
                                       config.restartable, config.asynchronous_start, {});

    if (config.trigger_crash_at_startup)
    {
        trigger_crash();
    }

    return CrashpadContext{
        .client = client,
        .is_started = status,
    };
}

void destroy_crashpad_context(const CrashpadContext& crashpad_context)
{
    Assert(crashpad_context.is_started);

    if (crashpad_context.is_started)
    {
        delete crashpad_context.client;
    }
}

namespace
{
    bool match(const char* lhs, const char* rhs)
    {
        return strcmp(lhs, rhs) == 0;
    }

    int crashpad_config_reader_callback(void* user_ptr, const char* section, const char* name, const char* value)
    {
        CrashpadConfig* config = static_cast<CrashpadConfig*>(user_ptr);

        if (match(section, "crashpad"))
        {
            if (match(name, "enable_crash_upload"))
            {
                config->enable_crash_upload = atoi(value) != 0;
            }
            else if (match(name, "upload_server_address"))
            {
                config->upload_server_address = value;
            }
            else if (match(name, "upload_server_port"))
            {
                config->upload_server_port = atoi(value);
            }
            else if (match(name, "trigger_crash_at_startup"))
            {
                config->trigger_crash_at_startup = atoi(value) != 0;
            }
            else
            {
                AssertUnreachable(); // Unknown name
                return 0;
            }
        }
        else
        {
            AssertUnreachable(); // Unknown section
            return 0;
        }

        return 1;
    }
} // namespace

CrashpadConfig crashpad_parse_ini_config(const char* filename)
{
    CrashpadConfig config;

    // From the docs:
    // Returns 0 on success, line number of first error on parse error (doesn't
    // stop on first error), -1 on file open error, or -2 on memory allocation
    // error (only when INI_USE_STACK is zero).
    const i32 ini_parse_code = ini_parse(filename, crashpad_config_reader_callback, &config);

    Assert(ini_parse_code <= 0);  // Assert on parsing error
    Assert(ini_parse_code != -2); // Asset on memory error

    return config;
}
} // namespace Reaper
