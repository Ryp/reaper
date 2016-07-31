#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "renderer/mesh/ModelLoader.h"

TEST_CASE("Mesh Loading", "[mesh]")
{
    ModelLoader loader;
    MeshCache cache;

    SECTION("OBJ files")
    {
        loader.load("res/model/bunny.obj", cache);
        loader.load("res/model/quad.obj", cache);
        loader.load("res/model/ship.obj", cache);
    }
}
