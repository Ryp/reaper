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
    WindowCreationDescriptor windowDescriptor;
    windowDescriptor.title = "TestTitle";
    windowDescriptor.width = 800;
    windowDescriptor.height = 600;
    windowDescriptor.fullscreen = false;

    IWindow* window = createWindow(windowDescriptor);
    AbstractRenderer* renderer = nullptr;

    renderer = AbstractRenderer::createRenderer();

    CHECK(renderer != nullptr);

    renderer->startup(window);
    window->renderLoop(renderer);
    renderer->shutdown();

    delete renderer;
}

