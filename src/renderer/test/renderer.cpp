////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/Renderer.h"
#include "renderer/vulkan/PresentationSurface.h"

TEST_CASE("Renderer")
{
    Window  window;
    AbstractRenderer* renderer = nullptr;

    window.Create("Vulkan Test");

    renderer = AbstractRenderer::createRenderer();

    CHECK(renderer != nullptr);

    renderer->startup(&window);
    window.renderLoop(renderer);
    renderer->shutdown();

    delete renderer;
}
