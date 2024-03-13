////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "common/DebugLog.h"
#include "common/ReaperRoot.h"

#include "renderer/Renderer.h"

#include "audio/AudioBackend.h"

#include <profiling/Scope.h>

#include "GameLoop.h"

#include <core/Assert.h>

#if defined(REAPER_USE_GOOGLE_CRASHPAD)
#    include "CrashpadHandler.h"
#endif

#if defined(REAPER_USE_GOOGLE_BREAKPAD)
#    include "BreakpadHandler.h"
#endif

#if defined(REAPER_PLATFORM_WINDOWS)
#    include <shlobj.h>
#endif

namespace Reaper
{
namespace
{
    void start_engine(ReaperRoot& root)
    {
        root.log = new DebugLog(LogLevel::Info);

        log_info(root, "engine: start");

        create_renderer(root);

        AudioConfig audio_config;

        root.audio = new AudioBackend();
        *root.audio = create_audio_backend(root, audio_config);
    }

    void stop_engine(ReaperRoot& root)
    {
        log_info(root, "engine: stop");

        destroy_audio_backend(root, *root.audio);
        delete root.audio;

        destroy_renderer(root);

        delete root.log;
        root.log = nullptr;
    }
} // namespace
} // namespace Reaper

int main(int /*ac*/, char** /*av*/)
{
    using namespace Reaper;

#if defined(REAPER_USE_GOOGLE_BREAKPAD)
    BreakpadContext breakpad_context = create_breakpad_context();
#endif

#if defined(REAPER_USE_GOOGLE_CRASHPAD)
    const char* config_filename = "crashpad.ini";

    CrashpadConfig crashpad_config = crashpad_parse_ini_config(config_filename);

#    if defined(REAPER_PLATFORM_LINUX)
    crashpad_config.crash_handler_path = "build/external/crashpad/crashpad_handler";
    crashpad_config.dumps_path = "/tmp/neptune"; // FIXME
#    elif defined(REAPER_PLATFORM_WINDOWS)
    crashpad_config.crash_handler_path = L"build/external/crashpad/crashpad_handler.exe";

    PWSTR   path = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);
    Assert(SUCCEEDED(hr));

    crashpad_config.dumps_path = std::wstring(path) + L"/neptune"; // FIXME
    CoTaskMemFree(path);
#    endif

    CrashpadContext crashpad_context = create_crashpad_context(crashpad_config);
    Assert(crashpad_context.is_started);
#endif

    {
        ReaperRoot root = {};

        start_engine(root);

        execute_game_loop(root);

        stop_engine(root);
    }

#if defined(REAPER_USE_GOOGLE_CRASHPAD)
    destroy_crashpad_context(crashpad_context);
#endif

#if defined(REAPER_USE_GOOGLE_BREAKPAD)
    destroy_breakpad_context(breakpad_context);
#endif

    return 0;
}
