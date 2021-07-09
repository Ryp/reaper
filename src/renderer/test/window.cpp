////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/window/Window.h"

namespace Reaper
{
TEST_CASE("Window")
{
    IWindow* window = nullptr;

    SUBCASE("Create window")
    {
        WindowCreationDescriptor windowDescriptor;
        windowDescriptor.title = "TestTitle";
        windowDescriptor.width = 800;
        windowDescriptor.height = 600;
        windowDescriptor.fullscreen = false;

        window = createWindow(windowDescriptor);

        delete window;
        window = nullptr;
    }
}
} // namespace Reaper
