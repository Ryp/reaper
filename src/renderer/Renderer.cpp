////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Renderer.h"

#include "renderer/vulkan/VulkanRenderer.h"
#include "renderer/vulkan/SwapchainRendererBase.h"

AbstractRenderer* AbstractRenderer::createRenderer()
{
    return new OldRenderer();
}

bool create_renderer(ReaperRoot* root)
{
    VulkanRenderer* vulkanRenderer = new VulkanRenderer();

    Assert(root->renderer == nullptr);

    create_vulkan_renderer(*vulkanRenderer, *root, nullptr);

    Assert(vulkanRenderer != nullptr);

    root->renderer = vulkanRenderer;
    return true;
}

void destroy_renderer(ReaperRoot* root)
{
    Assert(root->renderer != nullptr);

    destroy_vulkan_renderer(*root->renderer, *root);

    delete root->renderer;
    root->renderer = nullptr;
}

