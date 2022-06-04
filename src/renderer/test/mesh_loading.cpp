////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include <doctest/doctest.h>

#include "mesh/ModelLoader.h"

TEST_CASE("Mesh Loading")
{
    SUBCASE("OBJ files")
    {
        ModelLoader::loadOBJ("res/model/bunny.obj");
        ModelLoader::loadOBJ("res/model/quad.obj");
        ModelLoader::loadOBJ("res/model/ship.obj");
    }
}
