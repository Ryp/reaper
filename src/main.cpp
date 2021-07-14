////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "common/DebugLog.h"
#include "common/ReaperRoot.h"

#include "renderer/Renderer.h"

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

        root.log = new DebugLog();

        log_info(root, "engine: start");

        create_renderer(root);
    }

    void stop_engine(ReaperRoot& root)
    {
        log_info(root, "engine: stop");

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

    Reaper::execute_game_loop(root, *root.renderer->backend);

    Reaper::stop_engine(root);

    return 0;
}
