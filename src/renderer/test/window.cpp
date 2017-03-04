////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/window/Window.h"

TEST_CASE("Window")
{
    IWindow* window = nullptr;

    SUBCASE("Create window")
    {
        window = createWindow();
        window->create("Test");

        delete window;
        window = nullptr;
    }
}

