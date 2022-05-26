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

#include "core/Profile.h"

#include "GameLoop.h"

namespace Reaper
{
namespace
{
    void start_engine(ReaperRoot& root)
    {
#if defined(REAPER_USE_MICROPROFILE)
        MicroProfileOnThreadCreate("Main");
        MicroProfileSetEnableAllGroups(true);
        MicroProfileSetForceMetaCounters(true);
        MicroProfileStartContextSwitchTrace();
#endif

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

#if defined(REAPER_USE_MICROPROFILE)
        // MicroProfileDumpFileImmediately("profile.html", nullptr, nullptr);
        MicroProfileShutdown();
#endif
    }
} // namespace
} // namespace Reaper

int main(int /*ac*/, char** /*av*/)
{
    Reaper::ReaperRoot root = {};

    Reaper::start_engine(root);

    Reaper::execute_game_loop(root);

    Reaper::stop_engine(root);

    return 0;
}
