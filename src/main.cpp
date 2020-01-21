////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "common/DebugLog.h"
#include "common/ReaperRoot.h"

#include "renderer/Renderer.h"
#include "renderer/vulkan/Test.h"
#include "renderer/vulkan/VulkanRenderer.h"

#include "core/Profile.h"

// Log all of stdout and stderr to a file
#define REAPER_LOG_OUTPUT 1

int main(int /*ac*/, char** /*av*/)
{
#if REAPER_LOG_OUTPUT
    Assert(freopen("stdout.txt", "w", stdout));
    Assert(freopen("stderr.txt", "w", stderr));
#endif

#if defined(REAPER_USE_MICROPROFILE)
    MicroProfileOnThreadCreate("Main");
    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);
    MicroProfileStartContextSwitchTrace();
#endif

    {
        using namespace Reaper;

        ReaperRoot root = {};

        root.log = new DebugLog();

        log_info(root, "engine: startup");
        {
            if (create_renderer(root))
            {
                vulkan_test(root, *root.renderer->backend);
                destroy_renderer(root);
            }
        }
        log_info(root, "engine: shutdown");

        delete root.log;
        root.log = nullptr;
    }

#if defined(REAPER_USE_MICROPROFILE)
    MicroProfileDumpFileImmediately("profile.html", nullptr, nullptr);
    MicroProfileShutdown();
#endif

#if REAPER_LOG_OUTPUT
    Assert(fclose(stdout) == 0);
    Assert(fclose(stderr) == 0);
#endif

    return 0;
}
