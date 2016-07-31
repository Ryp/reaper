#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "renderer/texture/TextureLoader.h"

TEST_CASE("Texture Loading", "[texture]")
{
    TextureLoader loader;
    TextureCache cache;

    SECTION("DDS files")
    {
        loader.load("res/texture/bricks_diffuse.dds", cache);
        loader.load("res/texture/default.dds", cache);
        loader.load("res/texture/ship.dds", cache);
        loader.load("res/texture/abrahams.dds", cache);
    }
}
