#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "renderer/vulkan/PresentationSurface.h"

TEST_CASE("Window", "[window]")
{
    Window  window;

    SECTION("Create window")
    {
        window.Create("Test");
    }
}
