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

        if (create_renderer(&root))
        {
            REAPER_PROFILE_SCOPE("Ready", MP_GREEN);
            Sleep(1);
            {
                REAPER_PROFILE_SCOPE("Waiting", MP_GREEN);
                Sleep(1);
            }
            Sleep(1);

            REAPER_PROFILE_SCOPE("Destroying", MP_GREEN);
            destroy_renderer(&root);
        }

        delete root.log;
        root.log = nullptr;

        MicroProfileDumpFileImmediately("profile.html", nullptr, nullptr);
        Sleep(1);
        return 0;
    }
}

int main(int /*ac*/, char** /*av*/)
{
#if defined(REAPER_USE_MICROPROFILE)
    MicroProfileOnThreadCreate("Main");
    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);
#endif

    int r = reaper_main();

#if defined(REAPER_USE_MICROPROFILE)
    MicroProfileShutdown();
#endif

    return r;
}
