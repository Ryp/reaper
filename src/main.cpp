////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "renderer/Renderer.h"
#include "renderer/vulkan/PresentationSurface.h"

int main(int /*ac*/, char** /*av*/)
{
    Window window;
    AbstractRenderer* renderer = nullptr;

    window.Create("Vulkan Test");

    renderer = AbstractRenderer::createRenderer();

    renderer->startup(&window);
    window.renderLoop(renderer);
    renderer->shutdown();

    delete renderer;

    return 0;
}

