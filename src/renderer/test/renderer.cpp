#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "renderer/Renderer.h"
#include "renderer/vulkan/PresentationSurface.h"

TEST_CASE("Renderer", "[renderer]")
{
    Window  window;
    AbstractRenderer* renderer = nullptr;

    SECTION("Create window")
    {
        window.Create("Vulkan Test");
    }

    SECTION("Create renderer")
    {
        renderer = AbstractRenderer::createRenderer();

        CHECK(renderer != nullptr);
    }

    SECTION("Render")
    {
        renderer->startup(&window);
        window.renderLoop(renderer);
        renderer->shutdown();
    }

    SECTION("Delete renderer")
    {
        delete renderer;
    }
}
