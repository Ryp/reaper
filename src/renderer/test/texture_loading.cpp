////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/texture/TextureLoader.h"

TEST_CASE("Texture Loading")
{
    TextureLoader loader;
    TextureCache  cache(10_MiB);

    SUBCASE("DDS files")
    {
        loader.load("res/texture/bricks_diffuse.dds", cache);
        loader.load("res/texture/default.dds", cache);
        loader.load("res/texture/ship.dds", cache);
    }
}
