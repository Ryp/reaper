////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "RenderDoc.h"

#include "common/DebugLog.h"
#include "common/ReaperRoot.h"

#if defined(REAPER_USE_RENDERDOC)

#    include "core/DynamicLibrary.h"
#    include "core/Profile.h"

#    if defined(REAPER_PLATFORM_LINUX) || defined(REAPER_PLATFORM_MACOSX)
#        define RENDERDOC_LIB_NAME "librenderdoc.so"
#    elif defined(REAPER_PLATFORM_WINDOWS)
#        define RENDERDOC_LIB_NAME "renderdoc.dll"
#    endif

#    include <renderdoc.h>

namespace Reaper::RenderDoc
{
namespace
{
    constexpr RENDERDOC_Version RenderDocVersion = eRENDERDOC_API_Version_1_4_0;
    using RenderDocAPI = RENDERDOC_API_1_4_0;

    LibHandle     g_renderDocLib = nullptr;
    RenderDocAPI* g_renderDocAPI = nullptr;
} // namespace

void start_integration(ReaperRoot& root)
{
    REAPER_PROFILE_SCOPE("RenderDoc", MP_GREEN1);

    log_info(root, "renderdoc: starting integration");

    Assert(g_renderDocLib == nullptr);

    log_info(root, "renderdoc: loading {}", RENDERDOC_LIB_NAME);
    // FIXME Documentation recommends RTLD_NOW | RTLD_NOLOAD on linux
    g_renderDocLib = dynlib::load(RENDERDOC_LIB_NAME);

    pRENDERDOC_GetAPI pfn_RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dynlib::getSymbol(g_renderDocLib, "RENDERDOC_GetAPI");

    Assert(pfn_RENDERDOC_GetAPI(RenderDocVersion, (void**)&g_renderDocAPI) == 1);

    int major, minor, patch;
    g_renderDocAPI->GetAPIVersion(&major, &minor, &patch);

    log_info(root, "renderdoc: API version {}.{}.{}", major, minor, patch);
}

void stop_integration(ReaperRoot& root)
{
    REAPER_PROFILE_SCOPE("RenderDoc", MP_GREEN1);

    log_info(root, "renderdoc: stopping integration");

    Assert(g_renderDocLib != nullptr);

    // FIXME https://github.com/bkaradzic/bgfx/issues/1192
    // log_info(root, "renderdoc: unloading {}", RENDERDOC_LIB_NAME);
    // dynlib::close(g_renderDocLib);
    // g_renderDocLib = nullptr;
}
} // namespace Reaper::RenderDoc

#else

namespace Reaper::RenderDoc
{
void start_integration(ReaperRoot& root)
{
    log_info(root, "renderdoc: support disabled");
}

void stop_integration(ReaperRoot& root)
{
    log_info(root, "renderdoc: support disabled");
}
} // namespace Reaper::RenderDoc

#endif
