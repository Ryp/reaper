////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/vulkan/PresentationSurface.h"

TEST_CASE("Window")
{
    Window  window;

    SUBCASE("Create window")
    {
        window.Create("Test");
    }
}
