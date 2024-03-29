////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Renderer.h"

#include <core/Assert.h>

#include "renderer/renderdoc/RenderDoc.h"
#include "renderer/vulkan/Backend.h"

namespace Reaper
{
#if defined(REAPER_USE_RENDERDOC)
// FIXME add this to flags at app startup when proper support is added.
constexpr bool enable_renderdoc_integration = true;
#else
constexpr bool enable_renderdoc_integration = false;
#endif

void create_renderer(ReaperRoot& root)
{
    if (enable_renderdoc_integration)
        RenderDoc::start_integration(root);

    Assert(root.renderer == nullptr);

    root.renderer = new Renderer();
    root.renderer->backend = new VulkanBackend();

    create_vulkan_renderer_backend(root, *root.renderer->backend);
}

void destroy_renderer(ReaperRoot& root)
{
    Assert(root.renderer != nullptr);
    Assert(root.renderer->backend != nullptr);

    destroy_vulkan_renderer_backend(root, *root.renderer->backend);

    delete root.renderer->backend;
    delete root.renderer;
    root.renderer = nullptr;

    if (enable_renderdoc_integration)
        RenderDoc::stop_integration(root);
}
} // namespace Reaper
