#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "core/memory/CacheLine.h"

TEST_CASE("Cache line size", "[cache]")
{
    CHECK(cacheLineSize() == REAPER_CACHELINE_SIZE);
}
