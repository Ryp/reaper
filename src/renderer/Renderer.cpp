////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Renderer.h"

#include "renderer/vulkan/SwapchainRendererBase.h"
#include "renderer/vulkan/VulkanRenderer.h"

namespace Reaper
{
bool create_renderer(ReaperRoot* root)
{
    Assert(root != nullptr);
    Assert(root->renderer == nullptr);

    root->renderer = new Renderer();
    root->renderer->backend = new VulkanBackend();

    create_vulkan_renderer_backend(*root, *root->renderer->backend);

    return true;
}

void destroy_renderer(ReaperRoot* root)
{
    Assert(root != nullptr);
    Assert(root->renderer != nullptr);
    Assert(root->renderer->backend != nullptr);

    destroy_vulkan_renderer_backend(*root, *root->renderer->backend);

    delete root->renderer->backend;
    delete root->renderer;
    root->renderer = nullptr;
}

void run_renderer(ReaperRoot* root)
{
    Assert(root != nullptr);
    Assert(root->renderer != nullptr);
    Assert(root->renderer->backend != nullptr);

    test_vulkan_renderer(*root, *root->renderer->backend);
}
} // namespace Reaper
