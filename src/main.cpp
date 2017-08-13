////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "common/ReaperRoot.h"
#include "common/DebugLog.h"

#include "renderer/Renderer.h"
#include "renderer/vulkan/VulkanRenderer.h"

#include "core/Profile.h"

namespace
{
    int reaper_main()
    {
        ReaperRoot root = {};

        root.log = new DebugLog();

        log_info(root, "engine: startup");
        {
            if (create_renderer(&root))
            {

                destroy_renderer(&root);
            }
        }
        log_info(root, "engine: shutdown");

        delete root.log;
        root.log = nullptr;

        return 0;
    }
}

#if defined(REAPER_PLATFORM_LINUX)
#   include <unistd.h>
#endif

int main(int /*ac*/, char** /*av*/)
{
#if defined(REAPER_USE_MICROPROFILE)
    MicroProfileOnThreadCreate("Main");
    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);
    MicroProfileStartContextSwitchTrace();
#endif

    int r = reaper_main();

#if defined(REAPER_USE_MICROPROFILE)
    MicroProfileDumpFileImmediately("profile.html", nullptr, nullptr);
#   if defined(REAPER_PLATFORM_LINUX)
    usleep(420); // Hack until this is fixed: https://github.com/jonasmr/microprofile/issues/19
#   endif
    MicroProfileShutdown();
#endif

    return r;
}
