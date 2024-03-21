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
    using namespace Reaper;

    SUBCASE("OBJ files")
    {
        load_obj("res/model/bunny.obj");
        load_obj("res/model/quad.obj");
        load_obj("res/model/ship.obj");
    }
}
