////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "mesh/ModelLoader.h"

TEST_CASE("Mesh Loading")
{
    ModelLoader loader;
    MeshCache   cache;

    SUBCASE("Small OBJ files")
    {
        loader.load("res/model/quad.obj", cache);
        loader.load("res/model/ship.obj", cache);
    }

    SUBCASE("Big OBJ files") { loader.load("res/model/bunny.obj", cache); }
}
