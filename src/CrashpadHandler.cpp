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

namespace Reaper
{
CrashpadContext create_crashpad_context()
{
    // FIXME
#if defined(REAPER_PLATFORM_LINUX)
    auto database_dir = base::FilePath("/tmp/neptune");
    auto handler_path = base::FilePath("build/external/crashpad/crashpad_handler");
#elif defined(REAPER_PLATFORM_WINDOWS)
    auto database_dir = base::FilePath(L"C:\\tmp");
    auto handler_path = base::FilePath(L"crashpad_handler.exe");
#endif

    auto        metrics_dir = database_dir;
    const char* upload_url = "http://localhost:8000/upload";

    auto database = crashpad::CrashReportDatabase::Initialize(database_dir);

    if (database == nullptr)
    {
        return CrashpadContext{.client = nullptr, .is_started = false};
    }

    // FIXME Disable sending crashes for now as we're not finished implementing the crash collection server.
    Assert(database->GetSettings()->SetUploadsEnabled(false));

    auto* client = new crashpad::CrashpadClient();

    const bool restartable = true;
    const bool asynchronous_start = false;

    // NOTE: Crash has a low rate limit to be the least intrusive possible.
    // We can disable it for now.
    // By default some compression is used, but disabling it makes our server simpler to implement.
    bool status = client->StartHandler(handler_path, database_dir, metrics_dir, upload_url, {},
                                       {"--no-upload-gzip", "--no-rate-limit"}, restartable, asynchronous_start, {});

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
} // namespace Reaper
