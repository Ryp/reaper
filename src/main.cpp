////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "common/ReaperRoot.h"
#include "common/DebugLog.h"

#include "renderer/Renderer.h"
#include "renderer/vulkan/VulkanRenderer.h"

namespace
{
    int reaper_main()
    {
        ReaperRoot root = {};

        root.log = new DebugLog();

        if (create_renderer(&root))
        {
            // Do a barrel roll
            destroy_renderer(&root);
        }

        delete root.log;
        root.log = nullptr;

        return 0;
    }
}

int main(int /*ac*/, char** /*av*/)
{
    return reaper_main();
}

