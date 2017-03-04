////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/Renderer.h"
#include "renderer/vulkan/PresentationSurface.h"
#include "renderer/window/Window.h"

TEST_CASE("Renderer")
{
    IWindow* window = createWindow();
    AbstractRenderer* renderer = nullptr;

    window->create("Vulkan Test");

    renderer = AbstractRenderer::createRenderer();

    CHECK(renderer != nullptr);

    renderer->startup(window);
    window->renderLoop(renderer);
    renderer->shutdown();

    delete renderer;
}

